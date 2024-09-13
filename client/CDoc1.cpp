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

#include "CDoc1.h"

#include "Application.h"
#include "Crypto.h"
#include "QCryptoBackend.h"
#include "QSigner.h"
#include "Utils.h"
#include "dialogs/FileDialog.h"

#include "libcdoc/Crypto.h"

#include <QtCore/QBuffer>
#include <QtCore/QDebug>
#include <QtCore/QTemporaryFile>
#include <QtCore/QXmlStreamWriter>
#include <QtNetwork/QSslKey>

#include <openssl/x509.h>

const QString CDoc1::AES128CBC_MTH = QStringLiteral("http://www.w3.org/2001/04/xmlenc#aes128-cbc");
const QString CDoc1::AES192CBC_MTH = QStringLiteral("http://www.w3.org/2001/04/xmlenc#aes192-cbc");
const QString CDoc1::AES256CBC_MTH = QStringLiteral("http://www.w3.org/2001/04/xmlenc#aes256-cbc");
const QString CDoc1::AES128GCM_MTH = QStringLiteral("http://www.w3.org/2009/xmlenc11#aes128-gcm");
const QString CDoc1::AES192GCM_MTH = QStringLiteral("http://www.w3.org/2009/xmlenc11#aes192-gcm");
const QString CDoc1::AES256GCM_MTH = QStringLiteral("http://www.w3.org/2009/xmlenc11#aes256-gcm");
const QString CDoc1::RSA_MTH = QStringLiteral("http://www.w3.org/2001/04/xmlenc#rsa-1_5");
const QString CDoc1::KWAES256_MTH = QStringLiteral("http://www.w3.org/2001/04/xmlenc#kw-aes256");
const QString CDoc1::CONCATKDF_MTH = QStringLiteral("http://www.w3.org/2009/xmlenc11#ConcatKDF");
const QString CDoc1::AGREEMENT_MTH = QStringLiteral("http://www.w3.org/2009/xmlenc11#ECDH-ES");
const QString CDoc1::SHA256_MTH = QStringLiteral("http://www.w3.org/2001/04/xmlenc#sha256");
const QString CDoc1::SHA384_MTH = QStringLiteral("http://www.w3.org/2001/04/xmlenc#sha384");
const QString CDoc1::SHA512_MTH = QStringLiteral("http://www.w3.org/2001/04/xmlenc#sha512");

const QString CDoc1::DS = QStringLiteral("http://www.w3.org/2000/09/xmldsig#");
const QString CDoc1::DENC = QStringLiteral("http://www.w3.org/2001/04/xmlenc#");
const QString CDoc1::DSIG11 = QStringLiteral("http://www.w3.org/2009/xmldsig11#");
const QString CDoc1::XENC11 = QStringLiteral("http://www.w3.org/2009/xmlenc11#");

const QString CDoc1::MIME_ZLIB = QStringLiteral("http://www.isi.edu/in-noes/iana/assignments/media-types/application/zip");
const QString CDoc1::MIME_DDOC = QStringLiteral("http://www.sk.ee/DigiDoc/v1.3.0/digidoc.xsd");
const QString CDoc1::MIME_DDOC_OLD = QStringLiteral("http://www.sk.ee/DigiDoc/1.3.0/digidoc.xsd");

const QHash<QString, const EVP_CIPHER*> CDoc1::ENC_MTH{
	{AES128CBC_MTH, EVP_aes_128_cbc()}, {AES192CBC_MTH, EVP_aes_192_cbc()}, {AES256CBC_MTH, EVP_aes_256_cbc()},
	{AES128GCM_MTH, EVP_aes_128_gcm()}, {AES192GCM_MTH, EVP_aes_192_gcm()}, {AES256GCM_MTH, EVP_aes_256_gcm()},
};
const QHash<QString, QCryptographicHash::Algorithm> CDoc1::SHA_MTH{
	{SHA256_MTH, QCryptographicHash::Sha256}, {SHA384_MTH, QCryptographicHash::Sha384}, {SHA512_MTH, QCryptographicHash::Sha512}
};

CDoc1::CDoc1(const std::string &path)
	: QFile(QString::fromStdString(path))
{
	setLastError(t_("An error occurred while opening the document."));
	if(!open(QFile::ReadOnly))
		return;
	readXML(this, [this](QXmlStreamReader &xml) {
		// EncryptedData
		if(xml.name() == QLatin1String("EncryptedData"))
			mime = xml.attributes().value(QLatin1String("MimeType")).toString();
		// EncryptedData/EncryptionProperties/EncryptionProperty
		else if(xml.name() == QLatin1String("EncryptionProperty"))
		{
			for(const QXmlStreamAttribute &attr: xml.attributes())
			{
				if(attr.name() != QLatin1String("Name"))
					continue;
				if(attr.value() == QLatin1String("orig_file"))
				{
					QStringList fileparts = xml.readElementText().split('|');
					files.push_back({
						fileparts.value(0).toStdString(),
						fileparts.value(3).toStdString(),
						fileparts.value(2).toStdString(),
						fileparts.value(1).toUInt(),
						{}
					});
				}
				else
					properties[attr.value().toString()] = xml.readElementText();
			}
		}
		// EncryptedData/EncryptionMethod
		else if(xml.name() == QLatin1String("EncryptionMethod"))
			method = xml.attributes().value(QLatin1String("Algorithm")).toString();
		// EncryptedData/KeyInfo/EncryptedKey
		if(xml.name() != QLatin1String("EncryptedKey"))
			return;

        std::shared_ptr<CKeyCDoc1> key = CKeyCDoc1::newEmpty();
        key->label = xml.attributes().value(QLatin1String("Recipient")).toString();
		while(!xml.atEnd())
		{
			xml.readNext();
			if(xml.name() == QLatin1String("EncryptedKey") && xml.isEndElement())
				break;
			if(!xml.isStartElement())
				continue;
			if(xml.name() == QLatin1String("EncryptionMethod"))
			{
				auto method = xml.attributes().value(QLatin1String("Algorithm"));
				key->unsupported = std::max(key->unsupported, method != KWAES256_MTH && method != RSA_MTH);
			}
			// EncryptedData/KeyInfo/EncryptedKey/KeyInfo/AgreementMethod
			else if(xml.name() == QLatin1String("AgreementMethod"))
				key->unsupported = std::max(key->unsupported, xml.attributes().value(QLatin1String("Algorithm")) != AGREEMENT_MTH);
			// EncryptedData/KeyInfo/EncryptedKey/KeyInfo/AgreementMethod/KeyDerivationMethod
			else if(xml.name() == QLatin1String("KeyDerivationMethod"))
				key->unsupported = std::max(key->unsupported, xml.attributes().value(QLatin1String("Algorithm")) != CONCATKDF_MTH);
			// EncryptedData/KeyInfo/EncryptedKey/KeyInfo/AgreementMethod/KeyDerivationMethod/ConcatKDFParams
            else if(xml.name() == QLatin1String("ConcatKDFParams"))
			{
				QByteArray ba = QByteArray::fromHex(xml.attributes().value(QLatin1String("AlgorithmID")).toUtf8());
				key->AlgorithmID.assign(ba.cbegin(), ba.cend());
				if(key->AlgorithmID.front() == 0x00) key->AlgorithmID.erase(key->AlgorithmID.cbegin());
				ba = QByteArray::fromHex(xml.attributes().value(QLatin1String("PartyUInfo")).toUtf8());
				key->PartyUInfo.assign(ba.cbegin(), ba.cend());
				if(key->PartyUInfo.front() == 0x00) key->PartyUInfo.erase(key->PartyUInfo.cbegin());
				ba = QByteArray::fromHex(xml.attributes().value(QLatin1String("PartyVInfo")).toUtf8());
				key->PartyVInfo.assign(ba.cbegin(), ba.cend());
				if(key->PartyVInfo.front() == 0x00) key->PartyVInfo.erase(key->PartyVInfo.cbegin());
			}
			// EncryptedData/KeyInfo/EncryptedKey/KeyInfo/AgreementMethod/KeyDerivationMethod/ConcatKDFParams/DigestMethod
			else if(xml.name() == QLatin1String("DigestMethod"))
				key->concatDigest = xml.attributes().value(QLatin1String("Algorithm")).toString().toStdString();
			// EncryptedData/KeyInfo/EncryptedKey/KeyInfo/AgreementMethod/OriginatorKeyInfo/KeyValue/ECKeyValue/PublicKey
			else if(xml.name() == QLatin1String("PublicKey"))
			{
				xml.readNext();
				QByteArray qpk = fromBase64(xml.text());
				key->publicKey.assign(qpk.cbegin(), qpk.cend());
			}
			// EncryptedData/KeyInfo/EncryptedKey/KeyInfo/X509Data/X509Certificate
			else if(xml.name() == QLatin1String("X509Certificate"))
			{
				xml.readNext();
				QByteArray qder = fromBase64(xml.text());
				key->setCert(std::vector<uint8_t>(qder.cbegin(), qder.cend()));
			}
			// EncryptedData/KeyInfo/EncryptedKey/KeyInfo/CipherData/CipherValue
			else if(xml.name() == QLatin1String("CipherValue"))
			{
				xml.readNext();
				QByteArray bytes = fromBase64(xml.text());
				key->encrypted_fmk.assign(bytes.cbegin(), bytes.cend());
			}
		}
		keys.push_back(key);
	});
	if(!keys.empty())
		setLastError({});

	if(files.empty() && properties.contains(QStringLiteral("Filename")))
	{
		files.push_back({
			properties.value(QStringLiteral("Filename")).toStdString(),
			{},
			mime == MIME_ZLIB ? properties.value(QStringLiteral("OriginalMimeType")).toStdString() : mime.toStdString(),
			properties.value(QStringLiteral("OriginalSize")).toUInt(),
			{}
		});
	}
}

std::unique_ptr<CDoc1>
CDoc1::load(const std::string& path)
{
	auto cdoc = std::unique_ptr<CDoc1>(new CDoc1(path));
	if (cdoc->keys.empty())
		cdoc.reset();
	return cdoc;
}

bool CDoc1::decryptPayload(const std::vector<uint8_t> &fmk)
{
	if(!isOpen())
		return false;
	setLastError({});
	QByteArray data;
	seek(0);
	readXML(this, [&data](QXmlStreamReader &xml) {
		// EncryptedData/KeyInfo
		if(xml.name() == QLatin1String("KeyInfo"))
			xml.skipCurrentElement();
		// EncryptedData/CipherData/CipherValue
		else if(xml.name() == QLatin1String("CipherValue"))
		{
			xml.readNext();
			data = fromBase64(xml.text());
		}
	});
	if(data.isEmpty())
		return setLastError(t_("Error parsing document"));
	QByteArray qkey(reinterpret_cast<const char *>(fmk.data()), fmk.size());
	data = Crypto::cipher(ENC_MTH[method], qkey, data, false);
	if(data.isEmpty())
		return setLastError(t_("Failed to decrypt document"));

	// remove ANSIX923 padding
	if(data.size() > 0 && method == AES128CBC_MTH)
	{
		QByteArray ansix923(data[data.size()-1], 0);
		ansix923[ansix923.size()-1] = char(ansix923.size());
		if(data.right(ansix923.size()) == ansix923)
		{
			qCDebug(CRYPTO) << "Removing ANSIX923 padding size:" << ansix923.size();
			data.resize(data.size() - ansix923.size());
		}
	}

	if(mime == MIME_ZLIB)
	{
		// Add size header for qUncompress compatibilty
		unsigned origsize = std::max<unsigned>(properties.value(QStringLiteral("OriginalSize")).toUInt(), 1);
		qCDebug(CRYPTO) << "Decompressing zlib content size" << origsize;
		QByteArray size(4, 0);
		size[0] = char((origsize & 0xff000000) >> 24);
		size[1] = char((origsize & 0x00ff0000) >> 16);
		size[2] = char((origsize & 0x0000ff00) >> 8);
		size[3] = char((origsize & 0x000000ff));
		data = qUncompress(size + data);
		mime = properties[QStringLiteral("OriginalMimeType")];
	}

	if(mime == MIME_DDOC || mime == MIME_DDOC_OLD)
	{
		qCDebug(CRYPTO) << "Contains DDoc content" << mime;
		QTemporaryFile ddoc(QDir::tempPath() + "/XXXXXX");
		if(!ddoc.open())
			return setLastError(t_("Failed to create temporary files"));
		ddoc.write(data);
		ddoc.flush();
		ddoc.reset();
		files = readDDoc(&ddoc);
		return !files.empty();
	}

	auto buffer = std::make_shared<std::istringstream>(std::string(data.cbegin(), data.cend()));
	qCDebug(CRYPTO) << "Contains raw file" << mime;
	if(!files.empty())
	{
		files[0].size = data.size();
		files[0].stream = buffer;
	}
	else if(properties.contains(QStringLiteral("Filename")))
	{
		files.push_back({
			properties.value(QStringLiteral("Filename")).toStdString(),
			{},
			mime.toStdString(),
			data.size(),
			buffer,
		});
	}
	else
		return setLastError(t_("Error parsing document"));

	return !files.empty();
}

libcdoc::CKey::DecryptionStatus
CDoc1::canDecrypt(const libcdoc::Certificate &cert) const
{
	if(getDecryptionKey(cert))
		return libcdoc::CKey::DecryptionStatus::CAN_DECRYPT;
	return libcdoc::CKey::DecryptionStatus::CANNOT_DECRYPT;
}

std::shared_ptr<libcdoc::CKey> CDoc1::getDecryptionKey(const libcdoc::Certificate &cert) const
{
	for(std::shared_ptr<libcdoc::CKey> key: qAsConst(keys))
	{
		if (key->type != libcdoc::CKey::Type::CDOC1) continue;
		std::shared_ptr<libcdoc::CKeyCDoc1> k = std::static_pointer_cast<libcdoc::CKeyCDoc1>(key);
		if(!ENC_MTH.contains(method) ||
			k->cert != cert ||
            k->encrypted_fmk.isEmpty() ||
			k->unsupported)
			continue;
		if(cert.publicKey().algorithm() == QSsl::Rsa)
			return k;
		if(cert.publicKey().algorithm() == QSsl::Ec &&
			!k->publicKey.isEmpty())
			return k;
	}
	return {};
}

QByteArray CDoc1::fromBase64(QStringView data)
{
	unsigned int buf = 0;
	int nbits = 0;
	QByteArray result((data.size() * 3) / 4, Qt::Uninitialized);

	int offset = 0;
	for(const QChar &i: data)
	{
		int ch = int(i.toLatin1());
		int d {};

		if(ch >= 'A' && ch <= 'Z')
			d = ch - 'A';
		else if(ch >= 'a' && ch <= 'z')
			d = ch - 'a' + 26;
		else if(ch >= '0' && ch <= '9')
			d = ch - '0' + 52;
		else if(ch == '+')
			d = 62;
		else if(ch == '/')
			d = 63;
		else
			continue;

		buf = (buf << 6) | uint(d);
		nbits += 6;
		if(nbits >= 8)
		{
			nbits -= 8;
			result[offset++] = char(buf >> nbits);
			buf &= (1 << nbits) - 1;
		}
	}

	result.truncate(offset);
	return result;
}

std::vector<libcdoc::IOEntry> CDoc1::readDDoc(QIODevice *ddoc)
{
	qCDebug(CRYPTO) << "Parsing DDOC container";
	std::vector<libcdoc::IOEntry> files;
	readXML(ddoc, [&files] (QXmlStreamReader &x) {
		if(x.name() == QLatin1String("DataFile"))
		{
			libcdoc::IOEntry file;
			file.name = x.attributes().value(QLatin1String("Filename")).toString().normalized(QString::NormalizationForm_C).toStdString();
			file.id = x.attributes().value(QLatin1String("Id")).toString().normalized(QString::NormalizationForm_C).toStdString();
			file.mime = x.attributes().value(QLatin1String("MimeType")).toString().normalized(QString::NormalizationForm_C).toStdString();
			x.readNext();
			QByteArray content = fromBase64(x.text());
			auto buffer = std::make_shared<std::istringstream>(std::string(content.cbegin(), content.cend()));
			file.size = content.size();
			file.stream = buffer;
			files.push_back(std::move(file));
		}
	});
	return files;
}

void CDoc1::readXML(QIODevice *io, const std::function<void(QXmlStreamReader &)> &f)
{
	QXmlStreamReader r(io);
	while(!r.atEnd())
	{
		switch(r.readNext())
		{
		case QXmlStreamReader::DTD:
			qCWarning(CRYPTO) << "XML DTD Declarations are not supported";
			return;
		case QXmlStreamReader::EntityReference:
			qCWarning(CRYPTO) << "XML ENTITY References are not supported";
			return;
		case QXmlStreamReader::StartElement:
			f(r);
			break;
		default:
			break;
		}
	}
}

bool CDoc1::save(const std::string &path)
{
	setLastError({});
	QFile cdoc(QString::fromStdString(path));
	if(!cdoc.open(QFile::WriteOnly))
		return setLastError(cdoc.errorString().toStdString());

	QBuffer data;
	if(!data.open(QBuffer::WriteOnly))
		return false;

	QString mime, name;
	if(files.size() > 1)
	{
		qCDebug(CRYPTO) << "Creating DDoc container";
		writeDDoc(&data);
		mime = MIME_DDOC;
		name = QStringLiteral("payload.ddoc");
	}
	else
	{
		qCDebug(CRYPTO) << "Adding raw file";
		files[0].stream->seekg(0);
		copyIODevice(files[0].stream.get(), &data);
		mime = QString::fromStdString(files[0].mime);
		name = QString::fromStdString(files[0].name);
	}

	QString method = AES256GCM_MTH;
	QByteArray transportKey = Crypto::genKey(ENC_MTH[method]);
	if(transportKey.isEmpty())
		return setLastError(t_("Failed to generate transport key"));
#ifndef NDEBUG
	qDebug() << "ENC Transport Key" << transportKey.toHex();
#endif

	qCDebug(CRYPTO) << "Writing CDOC file ver 1.1 mime" << mime;
	QMultiHash<QString,QString> props {
		{ QStringLiteral("DocumentFormat"), QStringLiteral("ENCDOC-XML|1.1") },
		{ QStringLiteral("LibraryVersion"), Application::applicationName() + "|" + Application::applicationVersion() },
		{ QStringLiteral("Filename"), name },
	};
	for(const libcdoc::IOEntry &f: qAsConst(files))
		props.insert(QStringLiteral("orig_file"),
					 QStringLiteral("%1|%2|%3|%4").arg(QString::fromStdString(f.name)).arg(f.size).arg(QString::fromStdString(f.mime)).arg(QString::fromStdString(f.id)));

	QXmlStreamWriter w(&cdoc);
	w.setAutoFormatting(true);
	w.writeStartDocument();
	w.writeNamespace(DENC, QStringLiteral("denc"));
	writeElement(w, DENC, QStringLiteral("EncryptedData"), [&]{
		if(!mime.isEmpty())
			w.writeAttribute(QStringLiteral("MimeType"), mime);
		writeElement(w, DENC, QStringLiteral("EncryptionMethod"), {
		 {QStringLiteral("Algorithm"), method},
	 });
		w.writeNamespace(DS, QStringLiteral("ds"));
		writeElement(w, DS, QStringLiteral("KeyInfo"), [&]{
			for(std::shared_ptr<libcdoc::CKey> key: qAsConst(keys))
			{
				// Only certificate-based keys can be used in CDoc1
				if (key->type != libcdoc::CKey::Type::CERTIFICATE) return;
				std::shared_ptr<libcdoc::CKeyCert> ckey = std::static_pointer_cast<libcdoc::CKeyCert>(key);
				QSslCertificate kcert(QByteArray(reinterpret_cast<const char *>(ckey->cert.data()), ckey->cert.size()), QSsl::Der);
				writeElement(w, DENC, QStringLiteral("EncryptedKey"), [&]{
                    if(!ckey->label.isEmpty())
                        w.writeAttribute(QStringLiteral("Recipient"), ckey->label);
					QByteArray cipher;
					if(ckey->pk_type == libcdoc::CKey::PKType::RSA)
					{
						cipher = Crypto::encrypt(X509_get0_pubkey((const X509*)kcert.handle()), RSA_PKCS1_PADDING, transportKey);
						if(cipher.isEmpty())
							return;
						writeElement(w, DENC, QStringLiteral("EncryptionMethod"), {
						 {QStringLiteral("Algorithm"), RSA_MTH},
					 });
						writeElement(w, DS, QStringLiteral("KeyInfo"), [&]{
							writeElement(w, DS, QStringLiteral("X509Data"), [&]{
								writeBase64Element(w, DS, QStringLiteral("X509Certificate"), kcert.toDer());
							});
						});
					}
					else
					{
						EVP_PKEY *peerPKey = X509_get0_pubkey((const X509*)kcert.handle());
						auto priv = Crypto::genECKey(peerPKey);
						QByteArray sharedSecret = Crypto::derive(priv.get(), peerPKey);
						if(sharedSecret.isEmpty())
							return;

						QByteArray oid = Crypto::curve_oid(peerPKey);
						QByteArray SsDer = Crypto::toPublicKeyDer(priv.get());

						QString concatDigest = SHA384_MTH;
						switch((SsDer.size() - 1) / 2) {
						case 32: concatDigest = SHA256_MTH; break;
						case 48: concatDigest = SHA384_MTH; break;
						default: concatDigest = SHA512_MTH; break;
						}
						QByteArray encryptionKey = Crypto::concatKDF(SHA_MTH[concatDigest],
                            sharedSecret, props.value(QStringLiteral("DocumentFormat")).toUtf8() + SsDer + ckey->cert.toDer());
#ifndef NDEBUG
						qDebug() << "ENC Ss" << SsDer.toHex();
						qDebug() << "ENC Ksr" << sharedSecret.toHex();
						qDebug() << "ENC ConcatKDF" << encryptionKey.toHex();
#endif

						cipher = Crypto::aes_wrap(encryptionKey, transportKey);
						if(cipher.isEmpty())
							return;

						writeElement(w, DENC, QStringLiteral("EncryptionMethod"), {
							{QStringLiteral("Algorithm"), KWAES256_MTH},
						});
						writeElement(w, DS, QStringLiteral("KeyInfo"), [&]{
							writeElement(w, DENC, QStringLiteral("AgreementMethod"), {
								{QStringLiteral("Algorithm"), AGREEMENT_MTH},
							}, [&]{
								w.writeNamespace(XENC11, QStringLiteral("xenc11"));
								writeElement(w, XENC11, QStringLiteral("KeyDerivationMethod"), {
									{QStringLiteral("Algorithm"), CONCATKDF_MTH},
								}, [&]{
									writeElement(w, XENC11, QStringLiteral("ConcatKDFParams"), {
										{QStringLiteral("AlgorithmID"), QStringLiteral("00") + props.value(QStringLiteral("DocumentFormat")).toUtf8().toHex()},
										{QStringLiteral("PartyUInfo"), QStringLiteral("00") + SsDer.toHex()},
										{QStringLiteral("PartyVInfo"), QStringLiteral("00") + kcert.toDer().toHex()},
									}, [&]{
										writeElement(w, DS, QStringLiteral("DigestMethod"), {
											{QStringLiteral("Algorithm"), concatDigest},
										});
									});
								});
								writeElement(w, DENC, QStringLiteral("OriginatorKeyInfo"), [&]{
									writeElement(w, DS, QStringLiteral("KeyValue"), [&]{
										w.writeNamespace(DSIG11, QStringLiteral("dsig11"));
										writeElement(w, DSIG11, QStringLiteral("ECKeyValue"), [&]{
											writeElement(w, DSIG11, QStringLiteral("NamedCurve"), {
												{QStringLiteral("URI"), QStringLiteral("urn:oid:") + oid},
											});
											writeBase64Element(w, DSIG11, QStringLiteral("PublicKey"), SsDer);
										});
									});
								});
								writeElement(w, DENC, QStringLiteral("RecipientKeyInfo"), [&]{
									writeElement(w, DS, QStringLiteral("X509Data"), [&]{
										writeBase64Element(w, DS, QStringLiteral("X509Certificate"), kcert.toDer());
									});
								});
							});
						});
					}
					writeElement(w, DENC, QStringLiteral("CipherData"), [&]{
						writeBase64Element(w, DENC, QStringLiteral("CipherValue"), cipher);
					});
				});
			}});
		// This is actual content, for some weird reason named cipherData/cipherValue
		writeElement(w,DENC, QStringLiteral("CipherData"), [&]{
			writeBase64Element(w, DENC, QStringLiteral("CipherValue"),
				Crypto::cipher(ENC_MTH[method], transportKey, data.buffer(), true)
			);
		});
		writeElement(w, DENC, QStringLiteral("EncryptionProperties"), [&]{
			for(QMultiHash<QString,QString>::const_iterator i = props.constBegin(); i != props.constEnd(); ++i)
				writeElement(w, DENC, QStringLiteral("EncryptionProperty"), {
					{QStringLiteral("Name"), i.key()},
				}, [&]{
					w.writeCharacters(i.value());
				});
		});
	});
	w.writeEndDocument();
	return true;
}

auto toHex = [](const std::vector<uint8_t>& data) -> QString {
	QByteArray ba(reinterpret_cast<const char *>(data.data()), data.size());
	return ba.toHex();
};

std::vector<uint8_t>
CDoc1::getFMK(const libcdoc::CKey &key, const std::vector<uint8_t>& secret)
{
	if (key.type != libcdoc::CKey::Type::CDOC1) {
		setLastError(t_("Not a CDoc1 key"));
		return {};
	}
	const libcdoc::CKeyCDoc1& ckey = static_cast<const libcdoc::CKeyCDoc1&>(key);
	setLastError({});
	QByteArray decryptedKey = qApp->signer()->decrypt([&ckey](QCryptoBackend *backend) {
		if(ckey.pk_type == libcdoc::CKey::PKType::RSA) {
			QByteArray bytes(reinterpret_cast<const char *>(ckey.encrypted_fmk.data()), ckey.encrypted_fmk.size());
			return backend->decrypt(bytes, false);
		} else {
			QByteArray ba(reinterpret_cast<const char *>(ckey.publicKey.data()), ckey.publicKey.size());
			return backend->deriveConcatKDF(ba, SHA_MTH[QString::fromStdString(ckey.concatDigest)],
				int(KWAES_SIZE[QString::fromStdString(ckey.method)]),
				QByteArray(reinterpret_cast<const char *>(ckey.AlgorithmID.data()), ckey.AlgorithmID.size()),
				QByteArray(reinterpret_cast<const char *>(ckey.PartyUInfo.data()), ckey.PartyUInfo.size()),
				QByteArray(reinterpret_cast<const char *>(ckey.PartyVInfo.data()), ckey.PartyVInfo.size()));
		}
	});
	if(decryptedKey.isEmpty())
	{
		setLastError(t_("Failed to decrypt/derive key"));
		return {};
	}
	if(ckey.pk_type == libcdoc::CKey::PKType::RSA)
		return std::vector<uint8_t>(decryptedKey.cbegin(), decryptedKey.cend());
#ifndef NDEBUG
	qDebug() << "DEC Ss" << toHex(ckey.publicKey);
	qDebug() << "DEC ConcatKDF" << decryptedKey.toHex();
#endif
	QByteArray bytes(reinterpret_cast<const char *>(ckey.encrypted_fmk.data()), ckey.encrypted_fmk.size());
	QByteArray fmk = Crypto::aes_unwrap(decryptedKey, bytes);
	return std::vector<uint8_t>(fmk.cbegin(), fmk.cend());
}

void CDoc1::writeAttributes(QXmlStreamWriter &x, const QMap<QString,QString> &attrs)
{
	for(QMap<QString,QString>::const_iterator i = attrs.cbegin(), end = attrs.cend(); i != end; ++i)
		x.writeAttribute(i.key(), i.value());
}

void CDoc1::writeBase64Element(QXmlStreamWriter &x, const QString &ns, const QString &name, const QByteArray &data)
{
	x.writeStartElement(ns, name);
	for(int i = 0; i < data.size(); i+=48)
		x.writeCharacters(data.mid(i, 48).toBase64() + '\n');
	x.writeEndElement();
}

void CDoc1::writeDDoc(QIODevice *ddoc)
{
	qCDebug(CRYPTO) << "Creating DDOC container";
	QXmlStreamWriter x(ddoc);
	x.setAutoFormatting(true);
	x.writeStartDocument();
	x.writeDefaultNamespace(QStringLiteral("http://www.sk.ee/DigiDoc/v1.3.0#"));
	x.writeStartElement(QStringLiteral("SignedDoc"));
	writeAttributes(x, {
		{QStringLiteral("format"), QStringLiteral("DIGIDOC-XML")},
		{QStringLiteral("version"), QStringLiteral("1.3")},
	});

	for(const libcdoc::IOEntry &file: qAsConst(files))
	{
		x.writeStartElement(QStringLiteral("DataFile"));
		writeAttributes(x, {
			{QStringLiteral("ContentType"), QStringLiteral("EMBEDDED_BASE64")},
			{QStringLiteral("Filename"), QString::fromStdString(file.name)},
			{QStringLiteral("Id"), QString::fromStdString(file.id)},
			{QStringLiteral("MimeType"), QString::fromStdString(file.mime)},
			{QStringLiteral("Size"), QString::number(file.size)},
		});
		std::array<char, 48> buf{};
		file.stream->seekg(0);
		while (!file.stream->eof()) {
			file.stream->read(buf.data(), buf.size());
			size_t size = file.stream->gcount();
			if (size > 0) {
				x.writeCharacters(QByteArray::fromRawData(buf.data(), size).toBase64() + '\n');
			}
		}
		x.writeEndElement(); //DataFile
	}

	x.writeEndElement(); //SignedDoc
	x.writeEndDocument();
}

void CDoc1::writeElement(QXmlStreamWriter &x, const QString &ns, const QString &name, std::function<void()> &&f)
{
	x.writeStartElement(ns, name);
	if(f)
		f();
	x.writeEndElement();
}

void CDoc1::writeElement(QXmlStreamWriter &x, const QString &ns, const QString &name, const QMap<QString,QString> &attrs, std::function<void()> &&f)
{
	x.writeStartElement(ns, name);
	writeAttributes(x, attrs);
	if(f)
		f();
	x.writeEndElement();
}
