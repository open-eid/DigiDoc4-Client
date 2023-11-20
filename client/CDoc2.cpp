/*
 * QDigiDocClient
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#include "CDoc2.h"

#include "Application.h"
#include "CheckConnection.h"
#include "Colors.h"
#include "Crypto.h"
#include "QCryptoBackend.h"
#include "QSigner.h"
#include "Settings.h"
#include "TokenData.h"
#include "Utils.h"
#include "header_generated.h"
#include "effects/FadeInNotification.h"

#include <QtCore/QBuffer>
#include <QtCore/QDebug>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <QtCore/QtEndian>
#include <QtCore/QTemporaryFile>
#include <QtNetwork/QNetworkAccessManager>
#include <QtNetwork/QNetworkReply>
#include <QtNetwork/QSslKey>

#include <openssl/x509.h>

#include <zlib.h>

const QByteArray CDoc2::LABEL = "CDOC\x02";
const QByteArray CDoc2::CEK = "CDOC20cek";
const QByteArray CDoc2::HMAC = "CDOC20hmac";
const QByteArray CDoc2::KEK = "CDOC20kek";
const QByteArray CDoc2::KEKPREMASTER = "CDOC20kekpremaster";
const QByteArray CDoc2::PAYLOAD = "CDOC20payload";
const QByteArray CDoc2::SALT = "CDOC20salt";

namespace cdoc20 {
	bool checkConnection() {
		if(CheckConnection().check())
			return true;
		return dispatchToMain([] {
			auto *notification = new FadeInNotification(Application::mainWindow(),
				ria::qdigidoc4::colors::WHITE, ria::qdigidoc4::colors::MANTIS, 110);
			notification->start(QCoreApplication::translate("MainWindow", "Check internet connection"), 750, 3000, 1200);
			return false;
		});
	}

	QNetworkRequest req(const QString &keyserver_id, const QString &transaction_id = {}) {
#ifdef CONFIG_URL
		QJsonObject list = Application::confValue(QLatin1String("CDOC2-CONF")).toObject();
		QJsonObject data = list.value(keyserver_id).toObject();
		QString url = transaction_id.isEmpty() ?
			data.value(QLatin1String("POST")).toString(Settings::CDOC2_POST) :
			data.value(QLatin1String("FETCH")).toString(Settings::CDOC2_GET);
#else
		QString url = transaction_id.isEmpty() ? Settings::CDOC2_POST : Settings::CDOC2_GET;
#endif
		if(url.isEmpty())
			return QNetworkRequest{};
		QNetworkRequest req(QStringLiteral("%1/key-capsules%2").arg(url,
			transaction_id.isEmpty() ? QString(): QStringLiteral("/%1").arg(transaction_id)));
		req.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
		return req;
	}

	struct stream final: public QIODevice
	{
		static constexpr qint64 CHUNK = 16LL * 1024LL;
		QIODevice *io {};
		Crypto::Cipher *cipher {};
		z_stream s {};
		QByteArray buf;
		int flush = Z_NO_FLUSH;

		stream(QIODevice *_io, Crypto::Cipher *_cipher)
			: io(_io)
			, cipher(_cipher)
		{
			if(io->isReadable())
			{
				if(inflateInit2(&s, MAX_WBITS) == Z_OK)
					open(QIODevice::ReadOnly);
			}
			if(io->isWritable())
			{
				if(deflateInit(&s, Z_DEFAULT_COMPRESSION) == Z_OK)
					open(QIODevice::WriteOnly);
			}
		}

		~stream() final
		{
			if(io->isReadable())
				inflateEnd(&s);
			if(io->isWritable())
			{
				flush = Z_FINISH;
				writeData(nullptr, 0);
				deflateEnd(&s);
			}
		}

		bool isSequential() const final
		{
			return true;
		}

		qint64 bytesAvailable() const final
		{
			return (io->bytesAvailable() -  Crypto::Cipher::tagLen()) + buf.size() + QIODevice::bytesAvailable();
		}

		qint64 readData(char *data, qint64 maxlen) final
		{
			s.next_out = (Bytef*)data;
			s.avail_out = uInt(maxlen);
			std::array<char,CHUNK> in{};
			for(int res = Z_OK; s.avail_out > 0 && res == Z_OK;)
			{
				if(auto insize = io->bytesAvailable() -  Crypto::Cipher::tagLen(),
					size = io->read(in.data(), qMin<qint64>(insize, in.size())); size > 0)
				{
					if(!cipher->update(in.data(), int(size)))
						return -1;
					buf.append(in.data(), size);
				}
				s.next_in = (z_const Bytef*)buf.data();
				s.avail_in = uInt(buf.size());
				switch(res = inflate(&s, flush))
				{
				case Z_OK:
					buf = buf.right(s.avail_in);
					break;
				case Z_STREAM_END:
					buf.clear();
					break;
				default: return -1;
				}
			}
			return qint64(maxlen - s.avail_out);
		}

		qint64 writeData(const char *data, qint64 len) final
		{
			s.next_in = (z_const Bytef *)data;
			s.avail_in = uInt(len);
			std::array<char,CHUNK> out{};
			while(true) {
				s.next_out = (Bytef *)out.data();
				s.avail_out = out.size();
				int res = deflate(&s, flush);
				if(res == Z_STREAM_ERROR)
					return -1;
				if(auto size = out.size() - s.avail_out; size > 0)
				{
					if(!cipher->update(out.data(), int(size)) ||
						io->write(out.data(), size) != size)
						return -1;
				}
				if(res == Z_STREAM_END)
					break;
				if(flush == Z_FINISH)
					continue;
				if(s.avail_in == 0)
					break;
			}
			return len;
		}
	};

	struct TAR {
		std::unique_ptr<QIODevice> io;
		explicit TAR(std::unique_ptr<QIODevice> &&in)
			: io(std::move(in))
		{}

		bool save(const std::vector<CDoc::File> &files)
		{
			auto writeHeader = [this](Header &h, qint64 size) {
				h.chksum.fill(' ');
				toOctal(h.size, size);
				toOctal(h.chksum, h.checksum().first);
				return io->write((const char*)&h, Header::Size) == Header::Size;
			};
			auto writePadding = [this](qint64 size) {
				QByteArray pad(padding(size), 0);
				return io->write(pad) == pad.size();
			};
			auto toPaxRecord = [](const QByteArray &keyword, const QByteArray &value) {
				QByteArray record = ' ' + keyword + '=' + value + '\n';
				QByteArray result;
				for(auto len = record.size(); result.size() != len; ++len)
					result = QByteArray::number(len + 1) + record;
				return result;
			};
			for(const CDoc::File &file: files)
			{
				Header h {};
				QByteArray filename = file.name.toUtf8();
				QByteArray filenameTruncated = filename.left(h.name.size());
				std::copy(filenameTruncated.cbegin(), filenameTruncated.cend(), h.name.begin());

				if(filename.size() > 100 ||
					file.size > 07777777)
				{
					h.typeflag = 'x';
					QByteArray paxData;
					if(filename.size() > 100)
						paxData += toPaxRecord("path", filename);
					if(file.size > 07777777)
						paxData += toPaxRecord("size", QByteArray::number(file.size));
					if(!writeHeader(h, paxData.size()) ||
						io->write(paxData) != paxData.size() ||
						!writePadding(paxData.size()))
						return false;
				}

				h.typeflag = '0';
				if(!writeHeader(h, file.size))
					return false;
				file.data->seek(0);
				if(auto size = copyIODevice(file.data.get(), io.get()); size < 0 || !writePadding(size))
					return false;
			}
			return io->write((const char*)&Header::Empty, Header::Size) == Header::Size &&
				io->write((const char*)&Header::Empty, Header::Size) == Header::Size;
		}

		std::vector<CDoc::File> files(bool &warning) const
		{
			std::vector<CDoc::File> result;
			Header h {};
			auto readHeader = [&h, this] { return io->read((char*)&h, Header::Size) == Header::Size; };
			while(io->bytesAvailable() > 0)
			{
				if(!readHeader())
					return {};
				if(h.isNull())
				{
					if(!readHeader() && !h.isNull())
						return {};
					warning = io->bytesAvailable() > 0;
					return result;
				}
				if(!h.verify())
					return {};

				CDoc::File f;
				f.name = QString::fromUtf8(h.name.data(), std::min<int>(h.name.size(), int(strlen(h.name.data()))));
				f.size = fromOctal(h.size);
				if(h.typeflag == 'x')
				{
					QByteArray paxData = io->read(f.size);
					if(paxData.size() != f.size)
						return {};
					io->skip(padding(f.size));
					if(!readHeader() || h.isNull() || !h.verify())
						return {};
					f.size = fromOctal(h.size);
					for(const QByteArray &data: paxData.split('\n'))
					{
						if(data.isEmpty())
							break;
						const auto &headerValue = data.split('=');
						const auto &lenKeyword = headerValue[0].split(' ');
						if(data.size() + 1 != lenKeyword[0].toUInt())
							return {};
						if(lenKeyword[1] == "path")
							f.name = QString::fromUtf8(headerValue[1]);
						if(lenKeyword[1] == "size")
							f.size = headerValue[1].toUInt();
					}
				}

				if(h.typeflag == '0' || h.typeflag == 0)
				{
					if(f.size > 500L * 1024L * 1024L)
						f.data = std::make_unique<QTemporaryFile>();
					else
						f.data = std::make_unique<QBuffer>();
					f.data->open(QIODevice::ReadWrite);
					if(f.size != copyIODevice(io.get(), f.data.get(), f.size))
						return {};
					io->skip(padding(f.size));
					result.push_back(std::move(f));
				}
				else
					io->skip(f.size + padding(f.size));
			}
			return result;
		}

	private:
		struct Header {
			std::array<char,100> name;
			std::array<char,  8> mode;
			std::array<char,  8> uid;
			std::array<char,  8> gid;
			std::array<char, 12> size;
			std::array<char, 12> mtime;
			std::array<char,  8> chksum;
			char typeflag;
			std::array<char,100> linkname;
			std::array<char,  6> magic;
			std::array<char,  2> version;
			std::array<char, 32> uname;
			std::array<char, 32> gname;
			std::array<char,  8> devmajor;
			std::array<char,  8> devminor;
			std::array<char,155> prefix;
			std::array<char, 12> padding;

			std::pair<int64_t,int64_t> checksum() const
			{
				int64_t unsignedSum = 0;
				int64_t signedSum = 0;
				for (size_t i = 0, size = sizeof(Header); i < size; i++) {
					unsignedSum += ((unsigned char*) this)[i];
					signedSum += ((signed char*) this)[i];
				}
				return {unsignedSum, signedSum};
			}

			bool isNull() {
				return memcmp(this, &Empty, sizeof(Header)) == 0;
			}

			bool verify() {
				auto copy = chksum;
				chksum.fill(' ');
				auto checkSum = checksum();
				chksum.swap(copy);
				int64_t referenceChecksum = fromOctal(chksum);
				return referenceChecksum == checkSum.first ||
					   referenceChecksum == checkSum.second;
			}

			static const Header Empty;
			static const int Size;
		};

		template<std::size_t SIZE>
		static int64_t fromOctal(const std::array<char,SIZE> &data)
		{
			int64_t i = 0;
			for(const char c: data)
			{
				if(c < '0' || c > '7')
					continue;
				i <<= 3;
				i += c - '0';
			}
			return i;
		}

		template<std::size_t SIZE>
		static void toOctal(std::array<char,SIZE> &data, int64_t value)
		{
			data.fill(' ');
			for(auto it = data.rbegin() + 1; it != data.rend(); ++it)
			{
				*it = char(value & 7) + '0';
				value >>= 3;
			}
		}

		static constexpr int padding(int64_t size)
		{
			return Header::Size - size % Header::Size;
		}
	};

	const TAR::Header TAR::Header::Empty {};
	const int TAR::Header::Size = int(sizeof(TAR::Header));
}

CDoc2::CDoc2(const QString &path)
	: QFile(path)
{
	setLastError(QStringLiteral("Invalid CDoc 2.0 header"));
	uint32_t header_len = 0;
	if(!open(QFile::ReadOnly) ||
		read(LABEL.length()) != LABEL ||
		read((char*)&header_len, int(sizeof(header_len))) != int(sizeof(header_len)))
		return;
	header_data = read(qFromBigEndian<uint32_t>(header_len));
	if(header_data.size() != qFromBigEndian<uint32_t>(header_len))
		return;
	headerHMAC = read(KEY_LEN);
	if(headerHMAC.size() != KEY_LEN)
		return;
	noncePos = pos();
	flatbuffers::Verifier verifier(reinterpret_cast<const uint8_t*>(header_data.data()), header_data.size());
	if(!cdoc20::Header::VerifyHeaderBuffer(verifier))
		return;
	const auto *header = cdoc20::Header::GetHeader(header_data.constData());
	if(!header)
		return;
	if(header->payload_encryption_method() != cdoc20::Header::PayloadEncryptionMethod::CHACHA20POLY1305)
		return;
	const auto *recipients = header->recipients();
	if(!recipients)
		return;
	setLastError({});

	auto toByteArray = [](const flatbuffers::Vector<uint8_t> *data) {
		return data ? QByteArray((const char*)data->Data(), int(data->size())) : QByteArray();
	};
	auto toString = [](const flatbuffers::String *data) {
		return data ? QString::fromUtf8(data->c_str(), data->size()) : QString();
	};
	for(const auto *recipient: *recipients){
		if(recipient->fmk_encryption_method() != cdoc20::Header::FMKEncryptionMethod::XOR)
		{
			qWarning() << "Unsupported FMK encryption method: skipping";
			continue;
		}
		auto fillRecipient = [&] (auto key, bool isRSA) {
			CKey k(toByteArray(key->recipient_public_key()), isRSA);
			k.recipient = toString(recipient->key_label());
			k.cipher = toByteArray(recipient->encrypted_fmk());
			return k;
		};
		using cdoc20::Recipients::Capsule;
		switch(recipient->capsule_type())
		{
		case Capsule::ECCPublicKeyCapsule:
		{
			if(const auto *key = recipient->capsule_as_ECCPublicKeyCapsule())
			{
				if(key->curve() != cdoc20::Recipients::EllipticCurve::secp384r1)
				{
					qWarning() << "Unsupported ECC curve: skipping";
					continue;
				}
				CKey k = fillRecipient(key, false);
				k.publicKey = toByteArray(key->sender_public_key());
				keys.append(std::move(k));
			}
			break;
		}
		case Capsule::RSAPublicKeyCapsule:
		{
			if(const auto *key = recipient->capsule_as_RSAPublicKeyCapsule())
			{
				CKey k = fillRecipient(key, true);
				k.encrypted_kek = toByteArray(key->encrypted_kek());
				keys.append(std::move(k));
			}
			break;
		}
		case Capsule::KeyServerCapsule:
		{
			const auto *server = recipient->capsule_as_KeyServerCapsule();
			if(!server)
				qWarning() << "Unsupported Key Details: skipping";

			auto fillKeyServer = [&] (auto key, bool isRSA) {
				CKey k = fillRecipient(key, isRSA);
				k.keyserver_id = toString(server->keyserver_id());
				k.transaction_id = toString(server->transaction_id());
				return k;
			};
			switch(server->recipient_key_details_type())
			{
			case cdoc20::Recipients::ServerDetailsUnion::ServerEccDetails:
			{
				if(const auto *eccDetails = server->recipient_key_details_as_ServerEccDetails())
				{
					if(eccDetails->curve() == cdoc20::Recipients::EllipticCurve::secp384r1)
						keys.append(fillKeyServer(eccDetails, false));
				}
				break;
			}
			case cdoc20::Recipients::ServerDetailsUnion::ServerRsaDetails:
			{
				if(const auto *rsaDetails = server->recipient_key_details_as_ServerRsaDetails())
					keys.append(fillKeyServer(rsaDetails, true));
				break;
			}
			default:
				qWarning() << "Unsupported Key Server Details: skipping";
			}
			break;
		}
		default:
			qWarning() << "Unsupported Key Details: skipping";
		}
	}
}

CKey CDoc2::canDecrypt(const QSslCertificate &cert) const
{
	return keys.value(keys.indexOf(CKey(cert)));
}

bool CDoc2::decryptPayload(const QByteArray &fmk)
{
	if(!isOpen() || noncePos == -1)
		return false;
	setLastError({});
	seek(noncePos);
	QByteArray cek = Crypto::expand(fmk, CEK);
	QByteArray nonce = read(NONCE_LEN);
#ifndef NDEBUG
	qDebug() << "cek" << cek.toHex();
	qDebug() << "nonce" << nonce.toHex();
#endif
	Crypto::Cipher dec(EVP_chacha20_poly1305(), cek, nonce, false);
	if(!dec.updateAAD(PAYLOAD + header_data + headerHMAC))
		return false;
	bool warning = false;
	files = cdoc20::TAR(std::unique_ptr<QIODevice>(new cdoc20::stream(this, &dec))).files(warning);
	if(warning)
		setLastError(tr("CDoc contains additional payload data that is not part of content"));
	QByteArray tag = read(16);
#ifndef NDEBUG
	qDebug() << "tag" << tag.toHex();
#endif
	dec.setTag(tag);
	if(!dec.result())
		files.clear();
	return !files.empty();
}

bool CDoc2::save(const QString &path)
{
	setLastError({});
	QByteArray fmk = Crypto::extract(Crypto::random(KEY_LEN), SALT);
	QByteArray cek = Crypto::expand(fmk, CEK);
	QByteArray hhk = Crypto::expand(fmk, HMAC);
#ifndef NDEBUG
	qDebug() << "fmk" << fmk.toHex();
	qDebug() << "cek" << cek.toHex();
	qDebug() << "hhk" << hhk.toHex();
#endif

	flatbuffers::FlatBufferBuilder builder;
	std::vector<flatbuffers::Offset<cdoc20::Header::RecipientRecord>> recipients;
	auto toVector = [&builder](const QByteArray &data) {
		return builder.CreateVector((const uint8_t*)data.data(), size_t(data.length()));
	};
	auto toString = [&builder](const QString &data) {
		QByteArray utf8 = data.toUtf8();
		return builder.CreateString(utf8.data(), size_t(utf8.length()));
	};
	auto sendToServer = [this](CKey &key, const QByteArray &recipient_id, const QByteArray &key_material, QLatin1String type) {
		key.keyserver_id = Settings::CDOC2_DEFAULT_KEYSERVER;
		if(key.keyserver_id.isEmpty())
			return setLastError(QStringLiteral("keyserver_id cannot be empty"));
		QNetworkRequest req = cdoc20::req(key.keyserver_id);
		if(req.url().isEmpty())
			return setLastError(QStringLiteral("No valid config found for keyserver_id: %1").arg(key.keyserver_id));
		if(!cdoc20::checkConnection())
			return false;
		QScopedPointer<QNetworkAccessManager,QScopedPointerDeleteLater> nam(CheckConnection::setupNAM(req, Settings::CDOC2_GET_CERT));
		QEventLoop e;
		QNetworkReply *reply = nam->post(req, QJsonDocument({
			{QLatin1String("recipient_id"), QLatin1String(recipient_id.toBase64())},
			{QLatin1String("ephemeral_key_material"), QLatin1String(key_material.toBase64())},
			{QLatin1String("capsule_type"), type},
		}).toJson());
		connect(reply, &QNetworkReply::finished, &e, &QEventLoop::quit);
		e.exec();
		if(reply->error() == QNetworkReply::NoError &&
			reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() == 201)
			key.transaction_id = QString::fromLatin1(reply->rawHeader("Location")).remove(QLatin1String("/key-capsules/"));
		else
			return setLastError(reply->errorString());
		if(key.transaction_id.isEmpty())
			return setLastError(QStringLiteral("Failed to post key capsule"));
		return true;
	};

	for(CKey &key: keys)
	{
		if(key.isRSA)
		{
			QByteArray kek = Crypto::random(fmk.size());
			QByteArray xor_key = Crypto::xor_data(fmk, kek);
			auto publicKey = Crypto::fromRSAPublicKeyDer(key.key);
			if(!publicKey)
				return false;
			QByteArray encrytpedKek = Crypto::encrypt(publicKey.get(), RSA_PKCS1_OAEP_PADDING, kek);
#ifndef NDEBUG
			qDebug() << "publicKeyDer" << key.key.toHex();
			qDebug() << "kek" << kek.toHex();
			qDebug() << "xor" << xor_key.toHex();
			qDebug() << "encrytpedKek" << encrytpedKek.toHex();
#endif
			if(!Settings::CDOC2_USE_KEYSERVER)
			{
				auto rsaPublicKey = cdoc20::Recipients::CreateRSAPublicKeyCapsule(builder,
					toVector(key.key), toVector(encrytpedKek));
				recipients.push_back(cdoc20::Header::CreateRecipientRecord(builder,
					cdoc20::Recipients::Capsule::RSAPublicKeyCapsule, rsaPublicKey.Union(),
					toString(key.recipient), toVector(xor_key), cdoc20::Header::FMKEncryptionMethod::XOR));
				continue;
			}

			if(!sendToServer(key, key.key, encrytpedKek, QLatin1String("rsa")))
				return false;
			auto rsaKeyServer = cdoc20::Recipients::CreateServerRsaDetails(builder, toVector(key.key));
			auto keyServer = cdoc20::Recipients::CreateKeyServerCapsule(builder,
				cdoc20::Recipients::ServerDetailsUnion::ServerRsaDetails,
				rsaKeyServer.Union(), toString(key.keyserver_id), toString(key.transaction_id));
			recipients.push_back(cdoc20::Header::CreateRecipientRecord(builder,
				cdoc20::Recipients::Capsule::KeyServerCapsule, keyServer.Union(),
				toString(key.recipient), toVector(xor_key), cdoc20::Header::FMKEncryptionMethod::XOR));
			continue;
		}

		auto publicKey = Crypto::fromECPublicKeyDer(key.key, NID_secp384r1);
		if(!publicKey)
			return false;
		auto ephKey = Crypto::genECKey(publicKey.get());
		QByteArray sharedSecret = Crypto::derive(ephKey.get(), publicKey.get());
		QByteArray ephPublicKeyDer = Crypto::toPublicKeyDer(ephKey.get());
		QByteArray kekPm = Crypto::extract(sharedSecret, KEKPREMASTER);
		QByteArray info = KEK + cdoc20::Header::EnumNameFMKEncryptionMethod(cdoc20::Header::FMKEncryptionMethod::XOR) + key.key + ephPublicKeyDer;
		QByteArray kek = Crypto::expand(kekPm, info, fmk.size());
		QByteArray xor_key = Crypto::xor_data(fmk, kek);
#ifndef NDEBUG
		qDebug() << "publicKeyDer" << key.key.toHex();
		qDebug() << "ephPublicKeyDer" << ephPublicKeyDer.toHex();
		qDebug() << "sharedSecret" << sharedSecret.toHex();
		qDebug() << "kekPm" << kekPm.toHex();
		qDebug() << "kek" << kek.toHex();
		qDebug() << "xor" << xor_key.toHex();
#endif
		if(!Settings::CDOC2_USE_KEYSERVER)
		{
			auto eccPublicKey = cdoc20::Recipients::CreateECCPublicKeyCapsule(builder,
				cdoc20::Recipients::EllipticCurve::secp384r1, toVector(key.key), toVector(ephPublicKeyDer));
			recipients.push_back(cdoc20::Header::CreateRecipientRecord(builder,
				cdoc20::Recipients::Capsule::ECCPublicKeyCapsule, eccPublicKey.Union(),
				toString(key.recipient), toVector(xor_key), cdoc20::Header::FMKEncryptionMethod::XOR));
			continue;
		}

		if(!sendToServer(key, key.key, ephPublicKeyDer, QLatin1String("ecc_secp384r1")))
			return false;
		auto eccKeyServer = cdoc20::Recipients::CreateServerEccDetails(builder,
			cdoc20::Recipients::EllipticCurve::secp384r1, toVector(key.key));
		auto keyServer = cdoc20::Recipients::CreateKeyServerCapsule(builder,
			cdoc20::Recipients::ServerDetailsUnion::ServerEccDetails,
			eccKeyServer.Union(), toString(key.keyserver_id), toString(key.transaction_id));
		recipients.push_back(cdoc20::Header::CreateRecipientRecord(builder,
			cdoc20::Recipients::Capsule::KeyServerCapsule, keyServer.Union(),
			toString(key.recipient), toVector(xor_key), cdoc20::Header::FMKEncryptionMethod::XOR));
	}

	auto offset = cdoc20::Header::CreateHeader(builder, builder.CreateVector(recipients),
		cdoc20::Header::PayloadEncryptionMethod::CHACHA20POLY1305);
	builder.Finish(offset);

	QByteArray header = QByteArray::fromRawData((const char*)builder.GetBufferPointer(), int(builder.GetSize()));
	QByteArray headerHMAC = Crypto::sign_hmac(hhk, header);
	QByteArray nonce = Crypto::random(NONCE_LEN);
#ifndef NDEBUG
	qDebug() << "hmac" << headerHMAC.toHex();
	qDebug() << "nonce" << nonce.toHex();
#endif
	Crypto::Cipher enc(EVP_chacha20_poly1305(), cek, nonce, true);
	enc.updateAAD(PAYLOAD + header + headerHMAC);
	auto header_len = qToBigEndian<uint32_t>(uint32_t(header.size()));
	remove();
	QFile file(path);
	if(!file.open(QFile::WriteOnly))
		return setLastError(file.errorString());
	file.write(LABEL);
	file.write((const char*)&header_len, int(sizeof(header_len)));
	file.write(header);
	file.write(headerHMAC);
	file.write(nonce);
	if(!cdoc20::TAR(std::unique_ptr<QIODevice>(new cdoc20::stream(&file, &enc))).save(files))
	{
		file.remove();
		return false;
	}
	if(!enc.result())
	{
		file.remove();
		return false;
	}
	QByteArray tag = enc.tag();
#ifndef NDEBUG
	qDebug() << "tag" << tag.toHex();
#endif
	file.write(tag);
	return true;
}

QByteArray CDoc2::transportKey(const CKey &_key)
{
	setLastError({});
	CKey key = _key;
	if(!key.transaction_id.isEmpty())
	{
		QNetworkRequest req = cdoc20::req(key.keyserver_id, key.transaction_id);
		if(req.url().isEmpty())
		{
			setLastError(QStringLiteral("No valid config found for keyserver_id: %1").arg(key.keyserver_id));
			return {};
		}
		if(!cdoc20::checkConnection())
			return {};
		auto authKey = dispatchToMain(&QSigner::key, qApp->signer());
		QScopedPointer<QNetworkAccessManager,QScopedPointerDeleteLater> nam(
			CheckConnection::setupNAM(req, qApp->signer()->tokenauth().cert(), authKey, Settings::CDOC2_POST_CERT));
		QEventLoop e;
		QNetworkReply *reply = nam->get(req);
		connect(reply, &QNetworkReply::finished, &e, &QEventLoop::quit);
		e.exec();
		if(authKey.handle())
			qApp->signer()->logout();
		if(reply->error() != QNetworkReply::NoError &&
			reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() != 201)
		{
			setLastError(reply->errorString());
			return {};
		}
		QJsonObject json = QJsonDocument::fromJson(reply->readAll()).object();
		QByteArray key_material = QByteArray::fromBase64(
			json.value(QLatin1String("ephemeral_key_material")).toString().toLatin1());
		if(json.value(QLatin1String("capsule_type")) == QLatin1String("rsa"))
			key.encrypted_kek = key_material;
		else
			key.publicKey = key_material;
	}
#ifndef NDEBUG
	qDebug() << "publicKeyDer" << key.key.toHex();
	qDebug() << "ephPublicKeyDer" << key.publicKey.toHex();
#endif
	QByteArray kek = qApp->signer()->decrypt([&key](QCryptoBackend *backend) {
		if(key.isRSA)
			return backend->decrypt(key.encrypted_kek, true);
		QByteArray kekPm = backend->deriveHMACExtract(key.publicKey, KEKPREMASTER, KEY_LEN);
#ifndef NDEBUG
		qDebug() << "kekPm" << kekPm.toHex();
#endif
		QByteArray info = KEK + cdoc20::Header::EnumNameFMKEncryptionMethod(cdoc20::Header::FMKEncryptionMethod::XOR) + key.key + key.publicKey;
		return Crypto::expand(kekPm, info, KEY_LEN);
	});
	if(kek.isEmpty())
	{
		setLastError(QStringLiteral("Failed to derive key"));
		return {};
	}
	QByteArray fmk = Crypto::xor_data(key.cipher, kek);
	QByteArray hhk = Crypto::expand(fmk, HMAC);
#ifndef NDEBUG
	qDebug() << "kek" << kek.toHex();
	qDebug() << "xor" << key.cipher.toHex();
	qDebug() << "fmk" << fmk.toHex();
	qDebug() << "hhk" << hhk.toHex();
	qDebug() << "hmac" << headerHMAC.toHex();
#endif
	if(Crypto::sign_hmac(hhk, header_data) == headerHMAC)
		return fmk;
	setLastError(QStringLiteral("CDoc 2.0 hash mismatch"));
	return {};
}

int CDoc2::version()
{
	return 2;
}
