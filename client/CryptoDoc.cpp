/*
 * QDigiDocCrypto
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

#include "CryptoDoc.h"

#include "Application.h"
#include "TokenData.h"
#include "QCryptoBackend.h"
#include "QSigner.h"
#include "SslCertificate.h"
#include "dialogs/FileDialog.h"
#include "dialogs/WarningDialog.h"

#include <QDebug>
#include <QtCore/QBuffer>
#include <QtCore/QCryptographicHash>
#include <QtCore/QDir>
#include <QtCore/QFileInfo>
#include <QtCore/QLoggingCategory>
#include <QtCore/QMimeData>
#include <QtCore/QRegularExpression>
#include <QtCore/QTemporaryFile>
#include <QtCore/QtEndian>
#include <QtCore/QThread>
#include <QtCore/QUrl>
#include <QtCore/QXmlStreamReader>
#include <QtCore/QXmlStreamWriter>
#include <QtGui/QDesktopServices>
#include <QtNetwork/QSslKey>

#include <openssl/aes.h>
#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/rand.h>
#include <openssl/rsa.h>
#include <openssl/ecdh.h>
#include <openssl/x509.h>

#include <array>
#include <cmath>
#include <memory>

#if defined(Q_OS_WIN)
#include <qt_windows.h>
#include <fileapi.h>
#endif

using namespace ria::qdigidoc4;

using puchar = uchar *;
using pcuchar = const uchar *;

#define SCOPE(TYPE, DATA) std::unique_ptr<TYPE,decltype(&TYPE##_free)>(DATA, TYPE##_free)

Q_LOGGING_CATEGORY(CRYPTO,"CRYPTO")

class CryptoDoc::Private final: public QThread
{
	Q_OBJECT
public:
	struct File
	{
		QString name, id, mime, size;
		QByteArray data;
	};

	static QByteArray AES_wrap(const QByteArray &key, const QByteArray &data, bool encrypt);
	QByteArray crypto(const EVP_CIPHER *cipher, const QByteArray &data, bool encrypt);

	bool isEncryptedWarning() const;
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
	static QByteArray fromBase64(QStringView data);
#else
	static QByteArray fromBase64(const QStringRef &data);
#endif
	static bool opensslError(bool err);
	QByteArray readCDoc(QIODevice *cdoc, bool data);
	void readDDoc(QIODevice *ddoc);
	void run() final;
	static void showError(const QString &err, const QString &details = {})
	{
		WarningDialog::show(Application::mainWindow(), err, details);
	}
	static QString size(const QString &size)
	{
		bool converted = false;
		quint64 result = size.toUInt(&converted);
		return converted ? FileDialog::fileSize(result) : size;
	}
	inline void waitForFinished()
	{
		QEventLoop e;
		connect(this, &Private::finished, &e, &QEventLoop::quit);
		start();
		e.exec();
	}
	static void writeAttributes(QXmlStreamWriter &x, const QMap<QString,QString> &attrs)
	{
		for(QMap<QString,QString>::const_iterator i = attrs.cbegin(), end = attrs.cend(); i != end; ++i)
			x.writeAttribute(i.key(), i.value());
	}
	static void writeElement(QXmlStreamWriter &x, const QString &ns, const QString &name, const std::function<void()> &f = nullptr)
	{
		x.writeStartElement(ns, name);
		if(f)
			f();
		x.writeEndElement();
	}
	static void writeElement(QXmlStreamWriter &x, const QString &ns, const QString &name, const QMap<QString,QString> &attrs, const std::function<void()> &f = nullptr)
	{
		x.writeStartElement(ns, name);
		writeAttributes(x, attrs);
		if(f)
			f();
		x.writeEndElement();
	}
	static void writeBase64(QXmlStreamWriter &x, const QByteArray &data)
	{
		for(int i = 0; i < data.size(); i+=48)
			x.writeCharacters(data.mid(i, 48).toBase64() + "\n");
	}
	static void writeBase64Element(QXmlStreamWriter &x, const QString &ns, const QString &name, const QByteArray &data)
	{
		x.writeStartElement(ns, name);
		writeBase64(x, data);
		x.writeEndElement();
	}
	void writeCDoc(QIODevice *cdoc, const QByteArray &transportKey,
		const QByteArray &encryptedData, const QString &file, const QString &mime);
	void writeDDoc(QIODevice *ddoc);

	static const QString MIME_XML, MIME_ZLIB, MIME_DDOC, MIME_DDOC_OLD;
	static const QString DS, DENC, DSIG11, XENC11;
	static const QString AES128CBC_MTH, AES192CBC_MTH, AES256CBC_MTH, AES128GCM_MTH, AES192GCM_MTH, AES256GCM_MTH,
		RSA_MTH, KWAES128_MTH, KWAES192_MTH, KWAES256_MTH, CONCATKDF_MTH, AGREEMENT_MTH, SHA256_MTH, SHA384_MTH, SHA512_MTH;
	static const QHash<QString, const EVP_CIPHER*> ENC_MTH;
	static const QHash<QString, QCryptographicHash::Algorithm> SHA_MTH;
	static const QHash<QString, quint32> KWAES_SIZE;

	QString			method, mime, fileName, lastError;
	QByteArray		key;
	QHash<QString,QString> properties;
	QList<CKey>		keys;
	QList<File>		files;
	bool			isEncrypted = false;
	CDocumentModel	*documents = new CDocumentModel(this);
	QStringList		tempFiles;
};

const QString CryptoDoc::Private::MIME_XML = QStringLiteral("text/xml");
const QString CryptoDoc::Private::MIME_ZLIB = QStringLiteral("http://www.isi.edu/in-noes/iana/assignments/media-types/application/zip");
const QString CryptoDoc::Private::MIME_DDOC = QStringLiteral("http://www.sk.ee/DigiDoc/v1.3.0/digidoc.xsd");
const QString CryptoDoc::Private::MIME_DDOC_OLD = QStringLiteral("http://www.sk.ee/DigiDoc/1.3.0/digidoc.xsd");
const QString CryptoDoc::Private::DS = QStringLiteral("http://www.w3.org/2000/09/xmldsig#");
const QString CryptoDoc::Private::DENC = QStringLiteral("http://www.w3.org/2001/04/xmlenc#");
const QString CryptoDoc::Private::DSIG11 = QStringLiteral("http://www.w3.org/2009/xmldsig11#");
const QString CryptoDoc::Private::XENC11 = QStringLiteral("http://www.w3.org/2009/xmlenc11#");

const QString CryptoDoc::Private::AES128CBC_MTH = QStringLiteral("http://www.w3.org/2001/04/xmlenc#aes128-cbc");
const QString CryptoDoc::Private::AES192CBC_MTH = QStringLiteral("http://www.w3.org/2001/04/xmlenc#aes192-cbc");
const QString CryptoDoc::Private::AES256CBC_MTH = QStringLiteral("http://www.w3.org/2001/04/xmlenc#aes256-cbc");
const QString CryptoDoc::Private::AES128GCM_MTH = QStringLiteral("http://www.w3.org/2009/xmlenc11#aes128-gcm");
const QString CryptoDoc::Private::AES192GCM_MTH = QStringLiteral("http://www.w3.org/2009/xmlenc11#aes192-gcm");
const QString CryptoDoc::Private::AES256GCM_MTH = QStringLiteral("http://www.w3.org/2009/xmlenc11#aes256-gcm");
const QString CryptoDoc::Private::RSA_MTH = QStringLiteral("http://www.w3.org/2001/04/xmlenc#rsa-1_5");
const QString CryptoDoc::Private::KWAES128_MTH = QStringLiteral("http://www.w3.org/2001/04/xmlenc#kw-aes128");
const QString CryptoDoc::Private::KWAES192_MTH = QStringLiteral("http://www.w3.org/2001/04/xmlenc#kw-aes192");
const QString CryptoDoc::Private::KWAES256_MTH = QStringLiteral("http://www.w3.org/2001/04/xmlenc#kw-aes256");
const QString CryptoDoc::Private::CONCATKDF_MTH = QStringLiteral("http://www.w3.org/2009/xmlenc11#ConcatKDF");
const QString CryptoDoc::Private::AGREEMENT_MTH = QStringLiteral("http://www.w3.org/2009/xmlenc11#ECDH-ES");
const QString CryptoDoc::Private::SHA256_MTH = QStringLiteral("http://www.w3.org/2001/04/xmlenc#sha256");
const QString CryptoDoc::Private::SHA384_MTH = QStringLiteral("http://www.w3.org/2001/04/xmlenc#sha384");
const QString CryptoDoc::Private::SHA512_MTH = QStringLiteral("http://www.w3.org/2001/04/xmlenc#sha512");

const QHash<QString, const EVP_CIPHER*> CryptoDoc::Private::ENC_MTH{
	{AES128CBC_MTH, EVP_aes_128_cbc()}, {AES192CBC_MTH, EVP_aes_192_cbc()}, {AES256CBC_MTH, EVP_aes_256_cbc()},
	{AES128GCM_MTH, EVP_aes_128_gcm()}, {AES192GCM_MTH, EVP_aes_192_gcm()}, {AES256GCM_MTH, EVP_aes_256_gcm()},
};
const QHash<QString, QCryptographicHash::Algorithm> CryptoDoc::Private::SHA_MTH{
	{SHA256_MTH, QCryptographicHash::Sha256}, {SHA384_MTH, QCryptographicHash::Sha384}, {SHA512_MTH, QCryptographicHash::Sha512}
};
const QHash<QString, quint32> CryptoDoc::Private::KWAES_SIZE{{KWAES128_MTH, 16}, {KWAES192_MTH, 24}, {KWAES256_MTH, 32}};

QByteArray CryptoDoc::Private::AES_wrap(const QByteArray &key, const QByteArray &data, bool encrypt)
{
	QByteArray result;
	AES_KEY aes{};
	if(0 != (encrypt ?
		AES_set_encrypt_key(pcuchar(key.data()), key.length() * 8, &aes) :
		AES_set_decrypt_key(pcuchar(key.data()), key.length() * 8, &aes)))
		return result;
	result.resize(data.size() + 8);
	int size = encrypt ?
		AES_wrap_key(&aes, nullptr, puchar(result.data()), pcuchar(data.data()), uint(data.size())) :
		AES_unwrap_key(&aes, nullptr, puchar(result.data()), pcuchar(data.data()), uint(data.size()));
	if(size > 0)
		result.resize(size);
	else
		result.clear();
	return result;
}

QByteArray CryptoDoc::Private::crypto(const EVP_CIPHER *cipher, const QByteArray &data, bool encrypt)
{
	QByteArray iv, _data, tag;
	if(encrypt)
	{
#ifdef WIN32
		RAND_poll();
#else
		RAND_load_file("/dev/urandom", 1024);
#endif
		_data = data;
		iv.resize(EVP_CIPHER_iv_length(cipher));
		key.resize(EVP_CIPHER_key_length(cipher));
		std::array<uchar,PKCS5_SALT_LEN> salt{};
		std::array<uchar,128> indata{};
		RAND_bytes(salt.data(), salt.size());
		RAND_bytes(indata.data(), indata.size());
		if(opensslError(EVP_BytesToKey(cipher, EVP_sha256(), salt.data(), indata.data(), indata.size(),
				1, puchar(key.data()), puchar(iv.data())) <= 0))
			return {};
	}
	else
	{
		iv = data.left(EVP_CIPHER_iv_length(cipher));
		if(EVP_CIPHER_mode(cipher) == EVP_CIPH_GCM_MODE)
			tag = data.right(16);
		_data = data.mid(iv.size(), data.size() - iv.size() - tag.size());
	}

	auto ctx = SCOPE(EVP_CIPHER_CTX, EVP_CIPHER_CTX_new());
	if(opensslError(EVP_CipherInit(ctx.get(), cipher, pcuchar(key.constData()), pcuchar(iv.constData()), encrypt) <= 0))
		return {};

	int size = 0;
	QByteArray result(_data.size() + EVP_CIPHER_CTX_block_size(ctx.get()), Qt::Uninitialized);
	auto resultPointer = puchar(result.data()); //Detach only once
	if(opensslError(EVP_CipherUpdate(ctx.get(), resultPointer, &size, pcuchar(_data.constData()), _data.size()) <= 0))
		return {};

	if(!encrypt && EVP_CIPHER_mode(cipher) == EVP_CIPH_GCM_MODE)
		EVP_CIPHER_CTX_ctrl(ctx.get(), EVP_CTRL_GCM_SET_TAG, tag.size(), tag.data());

	int size2 = 0;
	if(opensslError(EVP_CipherFinal(ctx.get(), resultPointer + size, &size2) <= 0))
		return {};
	result.resize(size + size2);
	if(encrypt)
	{
		result.prepend(iv);
		if(EVP_CIPHER_mode(cipher) == EVP_CIPH_GCM_MODE)
		{
			tag = QByteArray(16, 0);
			EVP_CIPHER_CTX_ctrl(ctx.get(), EVP_CTRL_GCM_GET_TAG, tag.size(), tag.data());
			result.append(tag);
		}
	}
	return result;
}

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
QByteArray CryptoDoc::Private::fromBase64(QStringView data)
#else
QByteArray CryptoDoc::Private::fromBase64(const QStringRef &data)
#endif
{
	unsigned int buf = 0;
	int nbits = 0;
	QByteArray result((data.size() * 3) / 4, Qt::Uninitialized);

	int offset = 0;
	for(const QChar &i: data)
	{
		int ch = i.toLatin1();
		int d;

		if (ch >= 'A' && ch <= 'Z')
			d = ch - 'A';
		else if (ch >= 'a' && ch <= 'z')
			d = ch - 'a' + 26;
		else if (ch >= '0' && ch <= '9')
			d = ch - '0' + 52;
		else if (ch == '+')
			d = 62;
		else if (ch == '/')
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

bool CryptoDoc::Private::isEncryptedWarning() const
{
	if( fileName.isEmpty() )
		showError(CryptoDoc::tr("Container is not open"));
	if(isEncrypted)
		showError(CryptoDoc::tr("Container is encrypted"));
	return fileName.isEmpty() || isEncrypted;
}

bool CryptoDoc::Private::opensslError(bool err)
{
	if(err)
	{
		unsigned long errorCode = 0;
		while((errorCode =  ERR_get_error()) != 0)
			qCWarning(CRYPTO) << ERR_error_string(errorCode, nullptr);
	}
	return err;
}

void CryptoDoc::Private::run()
{
	if(!isEncrypted)
	{
		qCDebug(CRYPTO) << "Encrypt" << fileName;
		QBuffer data;
		if(!data.open(QBuffer::WriteOnly))
			return;

		QString mime, name;
		if(files.size() > 1)
		{
			qCDebug(CRYPTO) << "Creating DDoc container";
			writeDDoc(&data);
			mime = MIME_DDOC;
			name = QFileInfo(fileName).completeBaseName() + ".ddoc";
		}
		else
		{
			qCDebug(CRYPTO) << "Adding raw file";
			data.write(files[0].data);
			mime = files[0].mime;
			name = files[0].name;
		}
		method = AES256GCM_MTH;

		QByteArray result = crypto(ENC_MTH[method], data.data(), true);
		if(QFile cdoc(fileName); cdoc.open(QFile::WriteOnly))
			writeCDoc(&cdoc, key, result, name, mime);
	}
	else
	{
		qCDebug(CRYPTO) << "Decrypt" << fileName;
		QByteArray result;
		if(QFile cdoc(fileName); cdoc.open(QFile::ReadOnly))
			result = readCDoc(&cdoc, true);
		result = crypto(ENC_MTH[method], result, false);

		// remove ANSIX923 padding
		if(result.size() > 0 && method == AES128CBC_MTH)
		{
			QByteArray ansix923(result[result.size()-1], 0);
			ansix923[ansix923.size()-1] = char(ansix923.size());
			if(result.right(ansix923.size()) == ansix923)
			{
				qCDebug(CRYPTO) << "Removing ANSIX923 padding size:" << ansix923.size();
				result.resize(result.size() - ansix923.size());
			}
		}

		if(mime == MIME_ZLIB)
		{
			// Add size header for qUncompress compatibilty
			unsigned int origsize = std::max<unsigned>(properties[QStringLiteral("OriginalSize")].toUInt(), 1);
			qCDebug(CRYPTO) << "Decompressing zlib content size" << origsize;
			QByteArray size(4, 0);
			size[0] = char((origsize & 0xff000000) >> 24);
			size[1] = char((origsize & 0x00ff0000) >> 16);
			size[2] = char((origsize & 0x0000ff00) >> 8);
			size[3] = char((origsize & 0x000000ff));
			result = qUncompress(size + result);
			mime = properties[QStringLiteral("OriginalMimeType")];
		}

		if(mime == MIME_DDOC || mime == MIME_DDOC_OLD)
		{
			qCDebug(CRYPTO) << "Contains DDoc content" << mime;
			QTemporaryFile ddoc(QDir::tempPath() + "/XXXXXX");
			if(!ddoc.open())
			{
				lastError = CryptoDoc::tr("Failed to create temporary files<br />%1").arg(ddoc.errorString());
				return;
			}
			ddoc.write(result);
			ddoc.flush();
			ddoc.reset();
			result.clear();
			readDDoc(&ddoc);
		}
		else
		{
			qCDebug(CRYPTO) << "Contains raw file" << mime;
			if(!files.isEmpty())
				files[0].data = result;
			else if(properties.contains(QStringLiteral("Filename")))
			{
				File f;
				f.name = properties[QStringLiteral("Filename")];
				f.mime = mime;
				f.size = FileDialog::fileSize(quint64(result.size()));
				f.data = result;
				files.append(std::move(f));
			}
			else
				lastError = CryptoDoc::tr("Error parsing document");
		}
	}
	isEncrypted = !isEncrypted;
}

QByteArray CryptoDoc::Private::readCDoc(QIODevice *cdoc, bool data)
{
	qCDebug(CRYPTO) << "Parsing CDOC file, reading data only" << data;
	QXmlStreamReader xml(cdoc);

	if(!data)
	{
		files.clear();
		keys.clear();
		properties.clear();
		method.clear();
		mime.clear();
	}
	bool atStart = true;
	while( !xml.atEnd() )
	{
		switch(xml.readNext())
		{
		case QXmlStreamReader::StartElement:
			atStart = false;
			break;
		case QXmlStreamReader::DTD:
			showError(CryptoDoc::tr("An error occurred while opening the document."), QStringLiteral("XML DTD Declarations are not supported"));
			return {};
		case QXmlStreamReader::EntityReference:
			showError(CryptoDoc::tr("An error occurred while opening the document."), QStringLiteral("XML ENTITY References are not supported"));
			return {};
		default: continue;
		}

		if(data)
		{
			// EncryptedData/KeyInfo
			if(xml.name() == QLatin1String("KeyInfo"))
				xml.skipCurrentElement();
			// EncryptedData/CipherData/CipherValue
			else if(xml.name() == QLatin1String("CipherValue"))
			{
				xml.readNext();
				return fromBase64(xml.text());
			}
			continue;
		}

		// EncryptedData
		if(xml.name() == QLatin1String("EncryptedData"))
			mime = xml.attributes().value(QLatin1String("MimeType")).toString();
		// EncryptedData/EncryptionProperties/EncryptionProperty
		else if(xml.name() == QLatin1String("EncryptionProperty"))
		{
			for( const QXmlStreamAttribute &attr: xml.attributes() )
			{
				if(attr.name() != QLatin1String("Name"))
					continue;
				if(attr.value() == QLatin1String("orig_file"))
				{
					QStringList fileparts = xml.readElementText().split('|');
					File file;
					file.name = fileparts.value(0);
					file.size = size(fileparts.value(1));
					file.mime = fileparts.value(2);
					file.id = fileparts.value(3);
					files.append(std::move(file));
				}
				else
					properties[attr.value().toString()] = xml.readElementText();
			}
		}
		// EncryptedData/EncryptionMethod
		else if(xml.name() == QLatin1String("EncryptionMethod"))
			method = xml.attributes().value(QLatin1String("Algorithm")).toString();
		// EncryptedData/KeyInfo/EncryptedKey
		else if(xml.name() == QLatin1String("EncryptedKey"))
		{
			CKey key;
			key.id = xml.attributes().value(QLatin1String("Id")).toString();
			key.recipient = xml.attributes().value(QLatin1String("Recipient")).toString();
			while(!xml.atEnd())
			{
				xml.readNext();
				if(xml.name() == QLatin1String("EncryptedKey") && xml.isEndElement())
					break;
				if( !xml.isStartElement() )
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
					if(key.AlgorithmID[0] == char(0x00)) key.AlgorithmID.remove(0, 1);
					key.PartyUInfo = QByteArray::fromHex(xml.attributes().value(QLatin1String("PartyUInfo")).toUtf8());
					if(key.PartyUInfo[0] == char(0x00)) key.PartyUInfo.remove(0, 1);
					key.PartyVInfo = QByteArray::fromHex(xml.attributes().value(QLatin1String("PartyVInfo")).toUtf8());
					if(key.PartyVInfo[0] == char(0x00)) key.PartyVInfo.remove(0, 1);
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
					key.cert = QSslCertificate( fromBase64( xml.text() ), QSsl::Der );
				}
				// EncryptedData/KeyInfo/EncryptedKey/KeyInfo/CipherData/CipherValue
				else if(xml.name() == QLatin1String("CipherValue"))
				{
					xml.readNext();
					key.cipher = fromBase64(xml.text());
				}
			}
			keys.append(std::move(key));
		}
	}
	if(xml.hasError() && atStart)
		showError(CryptoDoc::tr("Failed to open the container. "
			"You need to update your ID-software in order to open CDOC2 containers. "
			"Install new ID-software from <a href='https://www.id.ee/en/article/install-id-software/'>www.id.ee</a>."), xml.errorString());
	else if(xml.hasError())
		showError(CryptoDoc::tr("An error occurred while opening the document."), xml.errorString());
	return {};
}

void CryptoDoc::Private::writeCDoc(QIODevice *cdoc, const QByteArray &transportKey,
	const QByteArray &encryptedData, const QString &file, const QString &mime)
{
#ifndef NDEBUG
	qDebug() << "ENC Transport Key" << transportKey.toHex();
#endif

	qCDebug(CRYPTO) << "Writing CDOC file ver 1.1 mime" << mime;
	QMultiHash<QString,QString> props {
		{ QStringLiteral("DocumentFormat"), QStringLiteral("ENCDOC-XML|1.1") },
		{ QStringLiteral("LibraryVersion"), Application::applicationName() + "|" + Application::applicationVersion() },
		{ QStringLiteral("Filename"), file },
	};
	QList<File> reverse = files;
	std::reverse(reverse.begin(), reverse.end());
	for(const File &f: qAsConst(reverse))
		props.insert(QStringLiteral("orig_file"), QStringLiteral("%1|%2|%3|%4").arg(f.name).arg(f.data.size()).arg(f.mime).arg(f.id));

	QXmlStreamWriter w(cdoc);
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
				if (k.cert.publicKey().algorithm() == QSsl::Rsa)
				{
					RSA *rsa = static_cast<RSA*>(k.cert.publicKey().handle());
					cipher.resize(RSA_size(rsa));
					if(opensslError(RSA_public_encrypt(transportKey.size(), pcuchar(transportKey.constData()),
						puchar(cipher.data()), rsa, RSA_PKCS1_PADDING) <= 0))
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
					QByteArray derCert = k.cert.toDer();
					auto pp = pcuchar(derCert.data());
					auto peerCert = SCOPE(X509, d2i_X509(nullptr, &pp, derCert.size()));
					EVP_PKEY *peerPKey = X509_get0_pubkey(peerCert.get());

					auto ctx = SCOPE(EVP_PKEY_CTX, EVP_PKEY_CTX_new(peerPKey, nullptr));
					EVP_PKEY *key = nullptr;
					if(opensslError(!ctx) ||
						opensslError(EVP_PKEY_keygen_init(ctx.get()) < 1) ||
						opensslError(EVP_PKEY_keygen(ctx.get(), &key)) < 1)
						return;
					auto pkey = SCOPE(EVP_PKEY, key);
					ctx = SCOPE(EVP_PKEY_CTX, EVP_PKEY_CTX_new(pkey.get(), nullptr));

					size_t sharedSecretLen = 0;
					if (opensslError(!ctx) ||
						opensslError(EVP_PKEY_derive_init(ctx.get()) <= 0) ||
						opensslError(EVP_PKEY_derive_set_peer(ctx.get(), peerPKey) <= 0) ||
						opensslError(EVP_PKEY_derive(ctx.get(), nullptr, &sharedSecretLen) <= 0))
						return;
					QByteArray sharedSecret(int(sharedSecretLen), 0);
					if(opensslError(EVP_PKEY_derive(ctx.get(), puchar(sharedSecret.data()), &sharedSecretLen) <= 0))
						return;

					QByteArray oid(50, 0);
					oid.resize(OBJ_obj2txt(oid.data(), oid.size(),
						OBJ_nid2obj(EC_GROUP_get_curve_name(EC_KEY_get0_group(EVP_PKEY_get0_EC_KEY(pkey.get())))), 1));
					QByteArray SsDer(i2d_PublicKey(pkey.get(), nullptr), 0);
					auto p = puchar(SsDer.data());
					i2d_PublicKey(pkey.get(), &p);

					const QString encryptionMethod = KWAES256_MTH;
					QString concatDigest = SHA384_MTH;
					switch((SsDer.size() - 1) / 2) {
					case 32: concatDigest = SHA256_MTH; break;
					case 48: concatDigest = SHA384_MTH; break;
					default: concatDigest = SHA512_MTH; break;
					}
					QByteArray encryptionKey = CryptoDoc::concatKDF(SHA_MTH[concatDigest], KWAES_SIZE[encryptionMethod],
						sharedSecret, props.value(QStringLiteral("DocumentFormat")).toUtf8() + SsDer + k.cert.toDer());
#ifndef NDEBUG
					qDebug() << "ENC Ss" << SsDer.toHex();
					qDebug() << "ENC Ksr" << sharedSecret.toHex();
					qDebug() << "ENC ConcatKDF" << encryptionKey.toHex();
#endif

					cipher = AES_wrap(encryptionKey, transportKey, true);
					if(opensslError(cipher.isEmpty()))
						return;

					writeElement(w, DENC, QStringLiteral("EncryptionMethod"), {
						{QStringLiteral("Algorithm"), encryptionMethod},
					});
					writeElement(w, DS, QStringLiteral("KeyInfo"), [&]{
						writeElement(w, DENC, QStringLiteral("AgreementMethod"), {
								{QStringLiteral("Algorithm"), AGREEMENT_MTH}
							}, [&]{
							w.writeNamespace(XENC11, QStringLiteral("xenc11"));
							writeElement(w, XENC11, QStringLiteral("KeyDerivationMethod"), {
									{QStringLiteral("Algorithm"), CONCATKDF_MTH},
								}, [&]{
								writeElement(w, XENC11, QStringLiteral("ConcatKDFParams"), {
									{QStringLiteral("AlgorithmID"), "00" + props.value("DocumentFormat").toUtf8().toHex()},
									{QStringLiteral("PartyUInfo"), "00" + SsDer.toHex()},
									{QStringLiteral("PartyVInfo"), "00" + k.cert.toDer().toHex()},
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
										writeElement(w, DSIG11, QStringLiteral("NamedCurve"), {{"URI", "urn:oid:" + oid}});
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
			writeBase64Element(w, DENC, QStringLiteral("CipherValue"), encryptedData);
		});
		writeElement(w, DENC, QStringLiteral("EncryptionProperties"), [&]{
			for(QMultiHash<QString,QString>::const_iterator i = props.constBegin(); i != props.constEnd(); ++i)
				writeElement(w, DENC, QStringLiteral("EncryptionProperty"), {
					{QStringLiteral("Name"), i.key()},
				}, [&]{ w.writeCharacters(i.value()); });
		});
	});
	w.writeEndDocument();
}

void CryptoDoc::Private::readDDoc(QIODevice *ddoc)
{
	qCDebug(CRYPTO) << "Parsing DDOC container";
	files.clear();
	QXmlStreamReader x(ddoc);
	while(!x.atEnd())
	{
		switch(x.readNext())
		{
		case QXmlStreamReader::StartElement: break;
		case QXmlStreamReader::DTD:
			qCWarning(CRYPTO) << "XML DTD Declarations are not supported";
			return;
		case QXmlStreamReader::EntityReference:
			qCWarning(CRYPTO) << "XML ENTITY References are not supported";
			return;
		default: continue;
		}

		if(x.name() == QLatin1String("DataFile"))
		{
			File file;
			file.name = x.attributes().value(QLatin1String("Filename")).toString().normalized(QString::NormalizationForm_C);
			file.id = x.attributes().value(QLatin1String("Id")).toString().normalized(QString::NormalizationForm_C);
			file.mime = x.attributes().value(QLatin1String("MimeType")).toString().normalized(QString::NormalizationForm_C);
			x.readNext();
			file.data = fromBase64( x.text() );
			file.size = FileDialog::fileSize(quint64(file.data.size()));
			files.append(std::move(file));
		}
	}
}

void CryptoDoc::Private::writeDDoc(QIODevice *ddoc)
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
			{QStringLiteral("Size"), QString::number(file.data.size())},
		});
		writeBase64(x, file.data);
		x.writeEndElement(); //DataFile
	}

	x.writeEndElement(); //SignedDoc
	x.writeEndDocument();
}


CDocumentModel::CDocumentModel(CryptoDoc::Private *doc)
: d( doc )
{}

bool CDocumentModel::addFile(const QString &file, const QString &mime)
{
	if( d->isEncryptedWarning() )
		return false;

	QFileInfo info(file);
	if(info.size() == 0)
	{
		d->showError(DocumentModel::tr("Cannot add empty file to the container."));
		return false;
	}
	if(info.size() > 120*1024*1024)
	{
		d->showError(tr("Added file(s) exceeds the maximum size limit of the container (∼120MB). "
			"<a href='https://www.id.ee/en/article/encrypting-large-120-mb-files/'>Read more about it</a>"));
		return false;
	}

	QString fileName(info.fileName());
	if(std::any_of(d->files.cbegin(), d->files.cend(),
			[&fileName](const auto &containerFile) { return containerFile.name == fileName; }))
	{
		d->showError(DocumentModel::tr("Cannot add the file to the envelope. File '%1' is already in container.")
			.arg(FileDialog::normalized(fileName)));
		return false;
	}

	QFile data(file);
	if(!data.open(QFile::ReadOnly))
		return false;
	CryptoDoc::Private::File f;
	f.id = QStringLiteral("D%1").arg(d->files.size());
	f.mime = mime;
	f.name = QFileInfo(file).fileName();
	f.data = data.readAll();
	f.size = FileDialog::fileSize(quint64(f.data.size()));
	d->files.append(std::move(f));
	emit added(FileDialog::normalized(d->files.last().name));
	return true;
}

void CDocumentModel::addTempReference(const QString &file)
{
	d->tempFiles.append(file);
}

QString CDocumentModel::copy(int row, const QString &dst) const
{
	const CryptoDoc::Private::File &file = d->files.at(row);
	if( QFile::exists( dst ) )
		QFile::remove( dst );
	if(QFile f(dst); !f.open(QFile::WriteOnly) || f.write(file.data) < 0)
	{
		d->showError(tr("Failed to save file '%1'").arg(dst));
		return {};
	}
	return dst;
}

QString CDocumentModel::data(int row) const
{
	return FileDialog::normalized(d->files.at(row).name);
}

QString CDocumentModel::fileSize(int /*row*/) const
{
	return {};
}

QString CDocumentModel::mime(int row) const
{
	return FileDialog::normalized(d->files.at(row).mime);
}

void CDocumentModel::open(int row)
{
	if(d->isEncrypted)
		return;
	QString path = FileDialog::tempPath(FileDialog::safeName(data(row)));
	if(!verifyFile(path))
		return;
	QFileInfo f(copy(row, path));
	if( !f.exists() )
		return;
	d->tempFiles.append(f.absoluteFilePath());
#if defined(Q_OS_WIN)
	::SetFileAttributesW(f.absoluteFilePath().toStdWString().c_str(), FILE_ATTRIBUTE_READONLY);
#else
	QFile::setPermissions(f.absoluteFilePath(), QFile::Permissions(QFile::Permission::ReadOwner));
#endif
	if(FileDialog::isSignedPDF(f.absoluteFilePath()))
		qApp->showClient({ f.absoluteFilePath() }, false, false, true);
	else
		QDesktopServices::openUrl(QUrl::fromLocalFile(f.absoluteFilePath()));
}

bool CDocumentModel::removeRows(int row, int count)
{
	if(d->isEncryptedWarning())
		return false;

	if( d->files.isEmpty() || row >= d->files.size() )
	{
		d->showError(DocumentModel::tr("Internal error"));
		return false;
	}

	for( int i = row + count - 1; i >= row; --i )
	{
		d->files.removeAt( i );
		emit removed(i);
	}
	return true;
}

int CDocumentModel::rowCount() const
{
	return d->files.size();
}

QString CDocumentModel::save(int row, const QString &path) const
{
	if(d->isEncrypted)
		return {};

	int zone = FileDialog::fileZone(d->fileName);
	QString fileName = copy(row, path);
	QFileInfo f(fileName);
	if(!f.exists())
		return {};
	FileDialog::setFileZone(fileName, zone);
	return fileName;
}



void CKey::setCert( const QSslCertificate &c )
{
	cert = c;
	recipient = [](const SslCertificate &c) {
		QString cn = c.subjectInfo(QSslCertificate::CommonName);
		QString o = c.subjectInfo(QSslCertificate::Organization);

		static const QRegularExpression rx(QStringLiteral("ESTEID \\((.*)\\)"));
		QRegularExpressionMatch match = rx.match(o);
		if(match.hasMatch())
			return QStringLiteral("%1,%2").arg(cn, match.captured(1));
		if(o == QLatin1String("ESTEID"))
			return QStringLiteral("%1,%2").arg(cn, CryptoDoc::tr("ID-CARD"));
		int certType = c.type();
		if(certType & SslCertificate::EResidentSubType)
			return QStringLiteral("%1,%2").arg(cn, CryptoDoc::tr("Digi-ID E-RESIDENT"));
		if(certType & SslCertificate::DigiIDType)
			return QStringLiteral("%1,%2").arg(cn, CryptoDoc::tr("Digi-ID"));
		if(certType & SslCertificate::EstEidType)
			return QStringLiteral("%1,%2").arg(cn, CryptoDoc::tr("ID-CARD"));
		return cn;
	}(c);
}



CryptoDoc::CryptoDoc( QObject *parent )
	: QObject(parent)
	, d(new Private)
{
	const_cast<QLoggingCategory&>(CRYPTO()).setEnabled(QtDebugMsg,
		QFile::exists(QStringLiteral("%1/%2.log").arg(QDir::tempPath(), Application::applicationName())));
}

CryptoDoc::~CryptoDoc() { clear(); delete d; }

bool CryptoDoc::addKey( const CKey &key )
{
	if( d->isEncryptedWarning() )
		return false;
	if( d->keys.contains( key ) )
	{
		d->showError(tr("Key already exists"));
		return false;
	}
	d->keys.append(key);
	return true;
}

bool CryptoDoc::canDecrypt(const QSslCertificate &cert)
{
	if(cert.isNull())
		return false;
	for(const CKey &k: qAsConst(d->keys))
	{
		if(!Private::ENC_MTH.contains(d->method) || k.cert != cert)
			continue;
		if(cert.publicKey().algorithm() == QSsl::Rsa &&
				!k.cipher.isEmpty() &&
				k.method == Private::RSA_MTH)
			return true;
		if(cert.publicKey().algorithm() == QSsl::Ec &&
				!k.publicKey.isEmpty() &&
				!k.cipher.isEmpty() &&
				Private::KWAES_SIZE.contains(k.method) &&
				k.derive ==  Private::CONCATKDF_MTH &&
				k.agreement ==  Private::AGREEMENT_MTH)
			return true;
	}
	return false;
}

QByteArray CryptoDoc::concatKDF(QCryptographicHash::Algorithm digestMethod,
	quint32 keyDataLen, const QByteArray &z, const QByteArray &otherInfo)
{
	if(z.isEmpty())
		return z;
	quint32 hashLen = 0;
	switch(digestMethod)
	{
	case QCryptographicHash::Sha256: hashLen = 32; break;
	case QCryptographicHash::Sha384: hashLen = 48; break;
	case QCryptographicHash::Sha512: hashLen = 64; break;
	default: return {};
	}
	auto reps = quint32(std::ceil(double(keyDataLen) / double(hashLen)));
	QCryptographicHash md(digestMethod);
	QByteArray key;
	for(quint32 i = 1; i <= reps; i++)
	{
		quint32 intToFourBytes = qToBigEndian(i);
		md.reset();
		md.addData((const char*)&intToFourBytes, 4);
		md.addData(z);
		md.addData(otherInfo);
		key += md.result();
	}
	return key.left(int(keyDataLen));
}

void CryptoDoc::clear( const QString &file )
{
	for(const QString &f: qAsConst(d->tempFiles))
	{
#if defined(Q_OS_WIN)
		//reset read-only attribute to enable delete file
		::SetFileAttributesW(f.toStdWString().c_str(), FILE_ATTRIBUTE_NORMAL);
#endif
		QFile::remove(f);
	}
	d->tempFiles.clear();
	d->isEncrypted = false;
	d->fileName = file;
	d->files.clear();
	d->keys.clear();
	d->properties.clear();
	d->method.clear();
	d->mime.clear();
}

ContainerState CryptoDoc::state() const
{
	return d->isEncrypted ? EncryptedContainer : UnencryptedContainer;
}

bool CryptoDoc::decrypt()
{
	if( d->fileName.isEmpty() )
	{
		d->showError(tr("Container is not open"));
		return false;
	}
	if(!d->isEncrypted)
		return true;

	CKey key;
	for(const CKey &k: qAsConst(d->keys))
	{
		if( qApp->signer()->tokenauth().cert() == k.cert )
		{
			key = k;
			break;
		}
	}
	if( key.cert.isNull() )
	{
		d->showError(tr("You do not have the key to decrypt this document"));
		return false;
	}

	bool isECDH = key.cert.publicKey().algorithm() == QSsl::Ec;
	QByteArray decryptedKey = qApp->signer()->decrypt([&key, &isECDH](QCryptoBackend *backend) {
		if(!isECDH)
			return backend->decrypt(key.cipher);
		return backend->deriveConcatKDF(key.publicKey, Private::SHA_MTH[key.concatDigest],
			int(Private::KWAES_SIZE[key.method]), key.AlgorithmID, key.PartyUInfo, key.PartyVInfo);
	});
	if(decryptedKey.isEmpty())
		return false;
	if(isECDH)
	{
#ifndef NDEBUG
		qDebug() << "DEC Ss" << key.publicKey.toHex();
		qDebug() << "DEC ConcatKDF" << decryptedKey.toHex();
#endif
		d->key = d->AES_wrap(decryptedKey, key.cipher, false);
		d->opensslError(d->key.isEmpty());
	}
	else // RSA decrypts directly transport key
		d->key = decryptedKey;
#ifndef NDEBUG
	qDebug() << "DEC transport" << d->key.toHex();
#endif

	d->waitForFinished();
	if( !d->lastError.isEmpty() )
		d->showError(d->lastError);

	return !d->isEncrypted;
}

DocumentModel* CryptoDoc::documentModel() const { return d->documents; }

bool CryptoDoc::encrypt( const QString &filename )
{
	if( !filename.isEmpty() )
		d->fileName = filename;
	if( d->fileName.isEmpty() )
	{
		d->showError(tr("Container is not open"));
		return false;
	}
	if(d->isEncrypted)
		return true;
	if( d->keys.isEmpty() )
	{
		d->showError(tr("No keys specified"));
		return false;
	}

	d->waitForFinished();
	if( !d->lastError.isEmpty() )
		d->showError(d->lastError);
	open(d->fileName);

	return d->isEncrypted;
}

QString CryptoDoc::fileName() const { return d->fileName; }

QList<CKey> CryptoDoc::keys() const
{
	return d->keys;
}

QList<QString> CryptoDoc::files()
{
	QList<QString> fileList;
	for(const Private::File &f: d->files)
		fileList.append(f.name);
	return fileList;
}

bool CryptoDoc::move(const QString &to)
{
	if(!d->isEncrypted)
	{
		d->fileName = to;
		return true;
	}

	return false;
}

bool CryptoDoc::open( const QString &file )
{
	clear(file);
	if(QFile cdoc(d->fileName); cdoc.open(QFile::ReadOnly))
		d->readCDoc(&cdoc, false);
	if(d->keys.isEmpty())
		return false;

	if(d->files.isEmpty() && d->properties.contains(QStringLiteral("Filename")))
	{
		Private::File f;
		f.name = d->properties[QStringLiteral("Filename")];
		f.mime = d->mime == Private::MIME_ZLIB ? d->properties[QStringLiteral("OriginalMimeType")] : d->mime;
		f.size = d->size(d->properties[QStringLiteral("OriginalSize")]);
		d->files.append(std::move(f));
	}

	d->isEncrypted = true;
	Application::addRecent( file );
	return !d->keys.isEmpty();
}

void CryptoDoc::removeKey( int id )
{
	if( !d->isEncryptedWarning() )
		d->keys.removeAt(id);
}

bool CryptoDoc::saveCopy(const QString &filename)
{
	if(QFileInfo(filename) == QFileInfo(d->fileName))
		return true;
	if(QFile::exists(filename))
		QFile::remove(filename);
	return QFile::copy(d->fileName, filename);
}

#include "CryptoDoc.moc"
