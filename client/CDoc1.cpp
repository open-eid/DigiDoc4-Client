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
const QString CDoc1::KWAES128_MTH = QStringLiteral("http://www.w3.org/2001/04/xmlenc#kw-aes128");
const QString CDoc1::KWAES192_MTH = QStringLiteral("http://www.w3.org/2001/04/xmlenc#kw-aes192");
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
const QHash<QString, quint32> CDoc1::KWAES_SIZE{{KWAES128_MTH, 16}, {KWAES192_MTH, 24}, {KWAES256_MTH, 32}};

CDoc1::CDoc1(const QString &path)
	: QFile(path)
{
	setLastError(CryptoDoc::tr("An error occurred while opening the document."));
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
						fileparts.value(0),
						fileparts.value(3),
						fileparts.value(2),
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

		CKey key;
		key.id = xml.attributes().value(QLatin1String("Id")).toString();
		key.recipient = xml.attributes().value(QLatin1String("Recipient")).toString();
		while(!xml.atEnd())
		{
			xml.readNext();
			if(xml.name() == QLatin1String("EncryptedKey") && xml.isEndElement())
				break;
			if(!xml.isStartElement())
				continue;
			// EncryptedData/KeyInfo/KeyName
			if(xml.name() == QLatin1String("KeyName"))
				key.name = xml.readElementText();
			// EncryptedData/KeyInfo/EncryptedKey/EncryptionMethod
			else if(xml.name() == QLatin1String("EncryptionMethod"))
				key.method = xml.attributes().value(QLatin1String("Algorithm")).toString();
			// EncryptedData/KeyInfo/EncryptedKey/KeyInfo/AgreementMethod
			else if(xml.name() == QLatin1String("AgreementMethod"))
				key.agreement = xml.attributes().value(QLatin1String("Algorithm")).toString();
			// EncryptedData/KeyInfo/EncryptedKey/KeyInfo/AgreementMethod/KeyDerivationMethod
			else if(xml.name() == QLatin1String("KeyDerivationMethod"))
				key.derive = xml.attributes().value(QLatin1String("Algorithm")).toString();
			// EncryptedData/KeyInfo/EncryptedKey/KeyInfo/AgreementMethod/KeyDerivationMethod/ConcatKDFParams
			else if(xml.name() == QLatin1String("ConcatKDFParams"))
			{
				key.AlgorithmID = QByteArray::fromHex(xml.attributes().value(QLatin1String("AlgorithmID")).toUtf8());
				if(key.AlgorithmID.front() == char(0x00)) key.AlgorithmID.remove(0, 1);
				key.PartyUInfo = QByteArray::fromHex(xml.attributes().value(QLatin1String("PartyUInfo")).toUtf8());
				if(key.PartyUInfo.front() == char(0x00)) key.PartyUInfo.remove(0, 1);
				key.PartyVInfo = QByteArray::fromHex(xml.attributes().value(QLatin1String("PartyVInfo")).toUtf8());
				if(key.PartyVInfo.front() == char(0x00)) key.PartyVInfo.remove(0, 1);
			}
			// EncryptedData/KeyInfo/EncryptedKey/KeyInfo/AgreementMethod/KeyDerivationMethod/ConcatKDFParams/DigestMethod
			else if(xml.name() == QLatin1String("DigestMethod"))
				key.concatDigest = xml.attributes().value(QLatin1String("Algorithm")).toString();
			// EncryptedData/KeyInfo/EncryptedKey/KeyInfo/AgreementMethod/OriginatorKeyInfo/KeyValue/ECKeyValue/PublicKey
			else if(xml.name() == QLatin1String("PublicKey"))
			{
				xml.readNext();
				key.publicKey = fromBase64(xml.text());
			}
			// EncryptedData/KeyInfo/EncryptedKey/KeyInfo/X509Data/X509Certificate
			else if(xml.name() == QLatin1String("X509Certificate"))
			{
				xml.readNext();
				key.setCert(QSslCertificate(fromBase64(xml.text()), QSsl::Der));
			}
			// EncryptedData/KeyInfo/EncryptedKey/KeyInfo/CipherData/CipherValue
			else if(xml.name() == QLatin1String("CipherValue"))
			{
				xml.readNext();
				key.cipher = fromBase64(xml.text());
			}
		}
		keys.append(std::move(key));
	});
	if(!keys.isEmpty())
		setLastError({});

	if(files.empty() && properties.contains(QStringLiteral("Filename")))
	{
		files.push_back({
			properties.value(QStringLiteral("Filename")),
			{},
			mime == MIME_ZLIB ? properties.value(QStringLiteral("OriginalMimeType")) : mime,
			properties.value(QStringLiteral("OriginalSize")).toUInt(),
			{}
		});
	}
}

bool CDoc1::decryptPayload(const QByteArray &key)
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
		return setLastError(CryptoDoc::tr("Error parsing document"));
	data = Crypto::cipher(ENC_MTH[method], key, data, false);
	if(data.isEmpty())
		return setLastError(CryptoDoc::tr("Failed to decrypt document"));

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
			return setLastError(CryptoDoc::tr("Failed to create temporary files<br />%1").arg(ddoc.errorString()));
		ddoc.write(data);
		ddoc.flush();
		ddoc.reset();
		files = readDDoc(&ddoc);
		return !files.empty();
	}

	auto buffer = std::make_unique<QBuffer>();
	buffer->setData(data);
	if(!buffer->open(QBuffer::ReadWrite))
		return false;
	qCDebug(CRYPTO) << "Contains raw file" << mime;
	if(!files.empty())
	{
		files[0].size = data.size();
		files[0].data = std::move(buffer);
	}
	else if(properties.contains(QStringLiteral("Filename")))
	{
		files.push_back({
			properties.value(QStringLiteral("Filename")),
			{},
			mime,
			data.size(),
			std::move(buffer),
		});
	}
	else
		return setLastError(CryptoDoc::tr("Error parsing document"));

	return !files.empty();
}

CKey CDoc1::canDecrypt(const QSslCertificate &cert) const
{
	for(const CKey &k: qAsConst(keys))
	{
		if(!ENC_MTH.contains(method) ||
			k.cert != cert ||
			k.cipher.isEmpty())
			continue;
		if(cert.publicKey().algorithm() == QSsl::Rsa &&
			k.method == RSA_MTH)
			return k;
		if(cert.publicKey().algorithm() == QSsl::Ec &&
			!k.publicKey.isEmpty() &&
			KWAES_SIZE.contains(k.method) &&
			k.derive == CONCATKDF_MTH &&
			k.agreement == AGREEMENT_MTH)
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

std::vector<CDoc::File> CDoc1::readDDoc(QIODevice *ddoc)
{
	qCDebug(CRYPTO) << "Parsing DDOC container";
	std::vector<File> files;
	readXML(ddoc, [&files] (QXmlStreamReader &x) {
		if(x.name() == QLatin1String("DataFile"))
		{
			File file;
			file.name = x.attributes().value(QLatin1String("Filename")).toString().normalized(QString::NormalizationForm_C);
			file.id = x.attributes().value(QLatin1String("Id")).toString().normalized(QString::NormalizationForm_C);
			file.mime = x.attributes().value(QLatin1String("MimeType")).toString().normalized(QString::NormalizationForm_C);
			x.readNext();
			auto buffer = std::make_unique<QBuffer>();
			buffer->setData(fromBase64(x.text()));
			buffer->open(QBuffer::ReadWrite);
			file.size = buffer->data().size();
			file.data = std::move(buffer);
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

bool CDoc1::save(const QString &path)
{
	setLastError({});
	QFile cdoc(path);
	if(!cdoc.open(QFile::WriteOnly))
		return setLastError(cdoc.errorString());

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
		files[0].data->seek(0);
		copyIODevice(files[0].data.get(), &data);
		mime = files[0].mime;
		name = files[0].name;
	}

	QString method = AES256GCM_MTH;
	QByteArray transportKey = Crypto::genKey(ENC_MTH[method]);
	if(transportKey.isEmpty())
		return setLastError(QStringLiteral("Failed to generate transport key"));
#ifndef NDEBUG
	qDebug() << "ENC Transport Key" << transportKey.toHex();
#endif

	qCDebug(CRYPTO) << "Writing CDOC file ver 1.1 mime" << mime;
	QMultiHash<QString,QString> props {
		{ QStringLiteral("DocumentFormat"), QStringLiteral("ENCDOC-XML|1.1") },
		{ QStringLiteral("LibraryVersion"), Application::applicationName() + "|" + Application::applicationVersion() },
		{ QStringLiteral("Filename"), name },
	};
	for(const File &f: qAsConst(files))
		props.insert(QStringLiteral("orig_file"), QStringLiteral("%1|%2|%3|%4").arg(f.name).arg(f.size).arg(f.mime).arg(f.id));

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
			for(const CKey &k: qAsConst(keys))
			{
				writeElement(w, DENC, QStringLiteral("EncryptedKey"), [&]{
					if(!k.id.isEmpty())
						w.writeAttribute(QStringLiteral("Id"), k.id);
					if(!k.recipient.isEmpty())
						w.writeAttribute(QStringLiteral("Recipient"), k.recipient);
					QByteArray cipher;
					if(k.isRSA)
					{
						cipher = Crypto::encrypt(X509_get0_pubkey((const X509*)k.cert.handle()), RSA_PKCS1_PADDING, transportKey);
						if(cipher.isEmpty())
							return;
						writeElement(w, DENC, QStringLiteral("EncryptionMethod"), {
							{QStringLiteral("Algorithm"), RSA_MTH},
						});
						writeElement(w, DS, QStringLiteral("KeyInfo"), [&]{
							if(!k.name.isEmpty())
								w.writeTextElement(DS, QStringLiteral("KeyName"), k.name);
							writeElement(w, DS, QStringLiteral("X509Data"), [&]{
								writeBase64Element(w, DS, QStringLiteral("X509Certificate"), k.cert.toDer());
							});
						});
					}
					else
					{
						EVP_PKEY *peerPKey = X509_get0_pubkey((const X509*)k.cert.handle());
						auto priv = Crypto::genECKey(peerPKey);
						QByteArray sharedSecret = Crypto::derive(priv.get(), peerPKey);
						if(sharedSecret.isEmpty())
							return;

						QByteArray oid = Crypto::curve_oid(peerPKey);
						QByteArray SsDer = Crypto::toPublicKeyDer(priv.get());

						const QString encryptionMethod = KWAES256_MTH;
						QString concatDigest = SHA384_MTH;
						switch((SsDer.size() - 1) / 2) {
						case 32: concatDigest = SHA256_MTH; break;
						case 48: concatDigest = SHA384_MTH; break;
						default: concatDigest = SHA512_MTH; break;
						}
						QByteArray encryptionKey = Crypto::concatKDF(SHA_MTH[concatDigest], KWAES_SIZE[encryptionMethod],
							sharedSecret, props.value(QStringLiteral("DocumentFormat")).toUtf8() + SsDer + k.cert.toDer());
#ifndef NDEBUG
						qDebug() << "ENC Ss" << SsDer.toHex();
						qDebug() << "ENC Ksr" << sharedSecret.toHex();
						qDebug() << "ENC ConcatKDF" << encryptionKey.toHex();
#endif

						cipher = Crypto::aes_wrap(encryptionKey, transportKey, true);
						if(cipher.isEmpty())
							return;

						writeElement(w, DENC, QStringLiteral("EncryptionMethod"), {
							{QStringLiteral("Algorithm"), encryptionMethod},
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
										{QStringLiteral("PartyVInfo"), QStringLiteral("00") + k.cert.toDer().toHex()},
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
										writeBase64Element(w, DS, QStringLiteral("X509Certificate"), k.cert.toDer());
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
		writeElement(w,DENC, QStringLiteral("CipherData"), [&]{
			writeBase64Element(w, DENC, QStringLiteral("CipherValue"),
				Crypto::cipher(ENC_MTH[method], transportKey, data.data(), true)
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

QByteArray CDoc1::transportKey(const CKey &key)
{
	setLastError({});
	QByteArray decryptedKey = qApp->signer()->decrypt([&key](QCryptoBackend *backend) {
		if(key.isRSA)
			return backend->decrypt(key.cipher, false);
		return backend->deriveConcatKDF(key.publicKey, SHA_MTH[key.concatDigest],
			int(KWAES_SIZE[key.method]), key.AlgorithmID, key.PartyUInfo, key.PartyVInfo);
	});
	if(decryptedKey.isEmpty())
	{
		setLastError(QStringLiteral("Failed to decrypt/derive key"));
		return {};
	}
	if(key.isRSA)
		return decryptedKey;
#ifndef NDEBUG
	qDebug() << "DEC Ss" << key.publicKey.toHex();
	qDebug() << "DEC ConcatKDF" << decryptedKey.toHex();
#endif
	return Crypto::aes_wrap(decryptedKey, key.cipher, false);
}

int CDoc1::version()
{
	return 1;
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

	for(const File &file: qAsConst(files))
	{
		x.writeStartElement(QStringLiteral("DataFile"));
		writeAttributes(x, {
			{QStringLiteral("ContentType"), QStringLiteral("EMBEDDED_BASE64")},
			{QStringLiteral("Filename"), file.name},
			{QStringLiteral("Id"), file.id},
			{QStringLiteral("MimeType"), file.mime},
			{QStringLiteral("Size"), QString::number(file.size)},
		});
		std::array<char, 48> buf{};
		file.data->seek(0);
		for(auto size = file.data->read(buf.data(), buf.size()); size > 0; size = file.data->read(buf.data(), buf.size()))
			x.writeCharacters(QByteArray::fromRawData(buf.data(), size).toBase64() + '\n');
		x.writeEndElement(); //DataFile
	}

	x.writeEndElement(); //SignedDoc
	x.writeEndDocument();
}

void CDoc1::writeElement(QXmlStreamWriter &x, const QString &ns, const QString &name, const std::function<void()> &f)
{
	x.writeStartElement(ns, name);
	if(f)
		f();
	x.writeEndElement();
}

void CDoc1::writeElement(QXmlStreamWriter &x, const QString &ns, const QString &name, const QMap<QString,QString> &attrs, const std::function<void()> &f)
{
	x.writeStartElement(ns, name);
	writeAttributes(x, attrs);
	if(f)
		f();
	x.writeEndElement();
}
