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

#include "client/Application.h"
#include "client/FileDialog.h"
#include "client/QSigner.h"

#include <common/Settings.h>
#include <common/SslCertificate.h>
#include <common/TokenData.h>

#include <QDebug>
#include <QtCore/QBuffer>
#include <QtCore/QCryptographicHash>
#include <QtCore/QDir>
#include <QtCore/QFileInfo>
#include <QtCore/QLoggingCategory>
#include <QtCore/QMimeData>
#include <QtCore/QProcessEnvironment>
#include <QtCore/QTemporaryFile>
#include <QtCore/QtEndian>
#include <QtCore/QThread>
#include <QtCore/QUrl>
#include <QtCore/QXmlStreamReader>
#include <QtCore/QXmlStreamWriter>
#include <QtGui/QDesktopServices>
#include <QtNetwork/QSslKey>
#include <QtWidgets/QMessageBox>

#include <openssl/aes.h>
#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/rand.h>
#include <openssl/rsa.h>
#include <openssl/ecdh.h>
#include <openssl/x509.h>

#include <cmath>
#include <memory>

using namespace ria::qdigidoc4;

typedef uchar *puchar;
typedef const uchar *pcuchar;

#define SCOPE(TYPE, VAR, DATA) std::unique_ptr<TYPE,decltype(&TYPE##_free)> VAR(DATA, TYPE##_free)

Q_LOGGING_CATEGORY(CRYPTO,"CRYPTO")

#if QT_VERSION < 0x050700
template <class T>
constexpr typename std::add_const<T>::type& qAsConst(T& t) noexcept
{
        return t;
}
#endif

class CryptoDocPrivate: public QThread
{
	Q_OBJECT
public:
	struct File
	{
		QString name, id, mime, size;
		QByteArray data;
	};

	QByteArray AES_wrap(const QByteArray &key, const QByteArray &data, bool encrypt);
	QByteArray crypto(const EVP_CIPHER *cipher, const QByteArray &data, bool encrypt);

	bool isEncryptedWarning();
	QByteArray fromBase64(const QStringRef &data);
	static bool opensslError(bool err);
	QByteArray readCDoc(QIODevice *cdoc, bool data);
	void readDDoc(QIODevice *ddoc);
	void run();
	void setLastError(const QString &err);
	QString size(const QString &size)
	{
		bool converted = false;
		quint64 result = size.toUInt(&converted);
		return converted ? FileDialog::fileSize(result) : size;
	}
	inline void waitForFinished()
	{
		QEventLoop e;
		connect(this, &CryptoDocPrivate::finished, &e, &QEventLoop::quit);
		start();
		e.exec();
	}
	inline void writeAttributes(QXmlStreamWriter &x, const QMap<QString,QString> &attrs)
	{
		for(QMap<QString,QString>::const_iterator i = attrs.cbegin(), end = attrs.cend(); i != end; ++i)
			x.writeAttribute(i.key(), i.value());
	}
	inline void writeElement(QXmlStreamWriter &x, const QString &ns, const QString &name, const std::function<void()> &f = nullptr)
	{
		x.writeStartElement(ns, name);
		if(f)
			f();
		x.writeEndElement();
	}
	inline void writeElement(QXmlStreamWriter &x, const QString &ns, const QString &name, const QMap<QString,QString> &attrs, const std::function<void()> &f = nullptr)
	{
		x.writeStartElement(ns, name);
		writeAttributes(x, attrs);
		if(f)
			f();
		x.writeEndElement();
	}
	inline void writeBase64(QXmlStreamWriter &x, const QByteArray &data)
	{
		for(int i = 0; i < data.size(); i+=48)
			x.writeCharacters(data.mid(i, 48).toBase64() + "\n");
	}
	inline void writeBase64Element(QXmlStreamWriter &x, const QString &ns, const QString &name, const QByteArray &data)
	{
		x.writeStartElement(ns, name);
		writeBase64(x, data);
		x.writeEndElement();
	}
	void writeCDoc(QIODevice *cdoc, const QByteArray &transportKey, const QByteArray &encryptedData,
		const QString &file, const QString &ver, const QString &mime);
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
	bool			hasSignature = false, encrypted = false;
	CDocumentModel	*documents = nullptr;
	QTemporaryFile	*ddoc = nullptr;
	QStringList		tempFiles;
};

const QString CryptoDocPrivate::MIME_XML = "text/xml";
const QString CryptoDocPrivate::MIME_ZLIB = "http://www.isi.edu/in-noes/iana/assignments/media-types/application/zip";
const QString CryptoDocPrivate::MIME_DDOC = "http://www.sk.ee/DigiDoc/v1.3.0/digidoc.xsd";
const QString CryptoDocPrivate::MIME_DDOC_OLD = "http://www.sk.ee/DigiDoc/1.3.0/digidoc.xsd";
const QString CryptoDocPrivate::DS = "http://www.w3.org/2000/09/xmldsig#";
const QString CryptoDocPrivate::DENC = "http://www.w3.org/2001/04/xmlenc#";
const QString CryptoDocPrivate::DSIG11 = "http://www.w3.org/2009/xmldsig11#";
const QString CryptoDocPrivate::XENC11 = "http://www.w3.org/2009/xmlenc11#";

const QString CryptoDocPrivate::AES128CBC_MTH = "http://www.w3.org/2001/04/xmlenc#aes128-cbc";
const QString CryptoDocPrivate::AES192CBC_MTH = "http://www.w3.org/2001/04/xmlenc#aes192-cbc";
const QString CryptoDocPrivate::AES256CBC_MTH = "http://www.w3.org/2001/04/xmlenc#aes256-cbc";
const QString CryptoDocPrivate::AES128GCM_MTH = "http://www.w3.org/2009/xmlenc11#aes128-gcm";
const QString CryptoDocPrivate::AES192GCM_MTH = "http://www.w3.org/2009/xmlenc11#aes192-gcm";
const QString CryptoDocPrivate::AES256GCM_MTH = "http://www.w3.org/2009/xmlenc11#aes256-gcm";
const QString CryptoDocPrivate::RSA_MTH = "http://www.w3.org/2001/04/xmlenc#rsa-1_5";
const QString CryptoDocPrivate::KWAES128_MTH = "http://www.w3.org/2001/04/xmlenc#kw-aes128";
const QString CryptoDocPrivate::KWAES192_MTH = "http://www.w3.org/2001/04/xmlenc#kw-aes192";
const QString CryptoDocPrivate::KWAES256_MTH = "http://www.w3.org/2001/04/xmlenc#kw-aes256";
const QString CryptoDocPrivate::CONCATKDF_MTH = "http://www.w3.org/2009/xmlenc11#ConcatKDF";
const QString CryptoDocPrivate::AGREEMENT_MTH = "http://www.w3.org/2009/xmlenc11#ECDH-ES";
const QString CryptoDocPrivate::SHA256_MTH = "http://www.w3.org/2001/04/xmlenc#sha256";
const QString CryptoDocPrivate::SHA384_MTH = "http://www.w3.org/2001/04/xmlenc#sha384";
const QString CryptoDocPrivate::SHA512_MTH = "http://www.w3.org/2001/04/xmlenc#sha512";

const QHash<QString, const EVP_CIPHER*> CryptoDocPrivate::ENC_MTH{
	{AES128CBC_MTH, EVP_aes_128_cbc()}, {AES192CBC_MTH, EVP_aes_192_cbc()}, {AES256CBC_MTH, EVP_aes_256_cbc()},
	{AES128GCM_MTH, EVP_aes_128_gcm()}, {AES192GCM_MTH, EVP_aes_192_gcm()}, {AES256GCM_MTH, EVP_aes_256_gcm()},
};
const QHash<QString, QCryptographicHash::Algorithm> CryptoDocPrivate::SHA_MTH{
	{SHA256_MTH, QCryptographicHash::Sha256}, {SHA384_MTH, QCryptographicHash::Sha384}, {SHA512_MTH, QCryptographicHash::Sha512}
};
const QHash<QString, quint32> CryptoDocPrivate::KWAES_SIZE{{KWAES128_MTH, 16}, {KWAES192_MTH, 24}, {KWAES256_MTH, 32}};

QByteArray CryptoDocPrivate::AES_wrap(const QByteArray &key, const QByteArray &data, bool encrypt)
{
	QByteArray result;
	AES_KEY aes;
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

QByteArray CryptoDocPrivate::crypto(const EVP_CIPHER *cipher, const QByteArray &data, bool encrypt)
{
	QByteArray iv, _data, tag;
	if(encrypt)
	{
#ifdef WIN32
		RAND_screen();
#else
		RAND_load_file("/dev/urandom", 1024);
#endif
		_data = data;
		iv.resize(EVP_CIPHER_iv_length(cipher));
		key.resize(EVP_CIPHER_key_length(cipher));
		uchar salt[PKCS5_SALT_LEN], indata[128];
		RAND_bytes(salt, sizeof(salt));
		RAND_bytes(indata, sizeof(indata));
		if(opensslError(EVP_BytesToKey(cipher, EVP_sha256(), salt, indata, sizeof(indata),
				1, puchar(key.data()), puchar(iv.data())) <= 0))
			return QByteArray();
	}
	else
	{
		iv = data.left(EVP_CIPHER_iv_length(cipher));
		if(EVP_CIPHER_mode(cipher) == EVP_CIPH_GCM_MODE)
			tag = data.right(16);
		_data = data.mid(iv.size(), data.size() - iv.size() - tag.size());
	}

	SCOPE(EVP_CIPHER_CTX, ctx, EVP_CIPHER_CTX_new());
	if(opensslError(EVP_CipherInit(ctx.get(), cipher, pcuchar(key.constData()), pcuchar(iv.constData()), encrypt) <= 0))
		return QByteArray();

	int size = 0;
	QByteArray result(_data.size() + EVP_CIPHER_CTX_block_size(ctx.get()), Qt::Uninitialized);
	puchar resultPointer = puchar(result.data()); //Detach only once
	if(opensslError(EVP_CipherUpdate(ctx.get(), resultPointer, &size, pcuchar(_data.constData()), _data.size()) <= 0))
		return QByteArray();

	if(!encrypt && EVP_CIPHER_mode(cipher) == EVP_CIPH_GCM_MODE)
		EVP_CIPHER_CTX_ctrl(ctx.get(), EVP_CTRL_GCM_SET_TAG, tag.size(), tag.data());

	int size2 = 0;
	if(opensslError(EVP_CipherFinal(ctx.get(), resultPointer + size, &size2) <= 0))
		return QByteArray();
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

QByteArray CryptoDocPrivate::fromBase64( const QStringRef &data )
{
	unsigned int buf = 0;
	int nbits = 0;
	QByteArray result((data.size() * 3) / 4, Qt::Uninitialized);

	int offset = 0;
	for( int i = 0; i < data.size(); ++i )
	{
		int ch = data.at(i).toLatin1();
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

bool CryptoDocPrivate::isEncryptedWarning()
{
	if( fileName.isEmpty() )
		setLastError( CryptoDoc::tr("Container is not open") );
	if( encrypted )
		setLastError( CryptoDoc::tr("Container is encrypted") );
	return fileName.isEmpty() || encrypted;
}

bool CryptoDocPrivate::opensslError(bool err)
{
	if(err)
	{
		unsigned long errorCode = 0;
		while((errorCode =  ERR_get_error()) != 0)
			qCWarning(CRYPTO) << ERR_error_string(errorCode, 0);
	}
	return err;
}

void CryptoDocPrivate::run()
{
	if( !encrypted )
	{
		qCDebug(CRYPTO) << "Encrypt" << fileName;
		QBuffer data;
		data.open(QBuffer::WriteOnly);

		QString mime, name;
		if(files.size() > 1 || Settings(qApp->applicationName()).value("cdocwithddoc", false).toBool())
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
// TODO? new check box "I would like to encrypt for recipients who are using an older DigiDoc3 Crypto\nsoftware (version 3.8 and earlier)." 
// to SettingsDialog (like in qdigidoc3).
		if(Settings(qApp->applicationName()).value("cdocwithddoc", false).toBool())
			method = AES128CBC_MTH;
		else
			method = AES256GCM_MTH;

		QString version = "1.1";
		if(method == AES128CBC_MTH) // add ANSIX923 padding
		{
			version = "1.0";
			QByteArray ansix923(16 - (data.size() % 16), 0);
			qCDebug(CRYPTO) << "Adding ANSIX923 padding size" << ansix923.size();
			ansix923[ansix923.size() - 1] = char(ansix923.size());
			data.write(ansix923);
			data.close();
		}

		QByteArray result = crypto(ENC_MTH[method], data.data(), true);
		QFile cdoc(fileName);
		cdoc.open(QFile::WriteOnly);
		writeCDoc(&cdoc, key, result, name, version, mime);
		cdoc.close();

		delete ddoc;
		ddoc = nullptr;
	}
	else
	{
		qCDebug(CRYPTO) << "Decrypt" << fileName;
		QFile cdoc(fileName);
		cdoc.open(QFile::ReadOnly);
		QByteArray result = readCDoc(&cdoc, true);
		cdoc.close();
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
			unsigned int origsize = std::max<unsigned>(properties["OriginalSize"].toUInt(), 1);
			qCDebug(CRYPTO) << "Decompressing zlib content size" << origsize;
			QByteArray size(4, 0);
			size[0] = char((origsize & 0xff000000) >> 24);
			size[1] = char((origsize & 0x00ff0000) >> 16);
			size[2] = char((origsize & 0x0000ff00) >> 8);
			size[3] = char((origsize & 0x000000ff));
			result = qUncompress(size + result);
			mime = properties["OriginalMimeType"];
		}

		if(mime == MIME_DDOC || mime == MIME_DDOC_OLD)
		{
			qCDebug(CRYPTO) << "Contains DDoc content" << mime;
			ddoc = new QTemporaryFile( QDir().tempPath() + "/XXXXXX" );
			if( !ddoc->open() )
			{
				lastError = CryptoDoc::tr("Failed to create temporary files<br />%1").arg( ddoc->errorString() );
				return;
			}
			ddoc->write(result);
			ddoc->flush();
			ddoc->reset();
			readDDoc(ddoc);
		}
		else
		{
			qCDebug(CRYPTO) << "Contains raw file" << mime;
			if(!files.isEmpty())
				files[0].data = result;
			else if(properties.contains("Filename"))
			{
				File f;
				f.name = properties["Filename"];
				f.mime = mime;
				f.size = FileDialog::fileSize(quint64(result.size()));
				f.data = result;
				files << f;
			}
			else
				lastError = CryptoDoc::tr("Error parsing document");
		}
	}
	encrypted = !encrypted;
}

void CryptoDocPrivate::setLastError( const QString &err )
{
	qApp->showWarning(err);
}

QByteArray CryptoDocPrivate::readCDoc(QIODevice *cdoc, bool data)
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
	while( !xml.atEnd() )
	{
		if( !xml.readNextStartElement() )
			continue;
		if(data)
		{
			// EncryptedData/KeyInfo
			if(xml.name() == "KeyInfo")
				xml.skipCurrentElement();
			// EncryptedData/CipherData/CipherValue
			else if(xml.name() == "CipherValue")
			{
				xml.readNext();
				return fromBase64(xml.text());
			}
			continue;
		}

		// EncryptedData
		else if( xml.name() == "EncryptedData")
			mime = xml.attributes().value("MimeType").toString();
		// EncryptedData/EncryptionProperties/EncryptionProperty
		else if( xml.name() == "EncryptionProperty" )
		{
			for( const QXmlStreamAttribute &attr: xml.attributes() )
			{
				if( attr.name() != "Name" )
					continue;
				if( attr.value() == "orig_file" )
				{
					QStringList fileparts = xml.readElementText().split("|");
					File file;
					file.name = fileparts.value(0);
					file.size = size(fileparts.value(1));
					file.mime = fileparts.value(2);
					file.id = fileparts.value(3);
					files << file;
				}
				else
					properties[attr.value().toString()] = xml.readElementText();
			}
		}
		// EncryptedData/EncryptionMethod
		else if( xml.name() == "EncryptionMethod" )
			method = xml.attributes().value("Algorithm").toString();
		// EncryptedData/KeyInfo/EncryptedKey
		else if( xml.name() == "EncryptedKey" )
		{
			CKey key;
			key.id = xml.attributes().value("Id").toString();
			key.recipient = xml.attributes().value("Recipient").toString();
			while(!xml.atEnd())
			{
				xml.readNext();
				if( xml.name() == "EncryptedKey" && xml.isEndElement() )
					break;
				if( !xml.isStartElement() )
					continue;
				// EncryptedData/KeyInfo/KeyName
				if(xml.name() == "KeyName")
					key.name = xml.readElementText();
				// EncryptedData/KeyInfo/EncryptedKey/EncryptionMethod
				else if(xml.name() == "EncryptionMethod")
					key.method = xml.attributes().value("Algorithm").toString();
				// EncryptedData/KeyInfo/EncryptedKey/KeyInfo/AgreementMethod
				else if(xml.name() == "AgreementMethod")
					key.agreement = xml.attributes().value("Algorithm").toString();
				// EncryptedData/KeyInfo/EncryptedKey/KeyInfo/AgreementMethod/KeyDerivationMethod
				else if(xml.name() == "KeyDerivationMethod")
					key.derive = xml.attributes().value("Algorithm").toString();
				// EncryptedData/KeyInfo/EncryptedKey/KeyInfo/AgreementMethod/KeyDerivationMethod/ConcatKDFParams
				else if(xml.name() == "ConcatKDFParams")
				{
					key.AlgorithmID = QByteArray::fromHex(xml.attributes().value("AlgorithmID").toUtf8());
					if(key.AlgorithmID[0] == char(0x00)) key.AlgorithmID.remove(0, 1);
					key.PartyUInfo = QByteArray::fromHex(xml.attributes().value("PartyUInfo").toUtf8());
					if(key.PartyUInfo[0] == char(0x00)) key.PartyUInfo.remove(0, 1);
					key.PartyVInfo = QByteArray::fromHex(xml.attributes().value("PartyVInfo").toUtf8());
					if(key.PartyVInfo[0] == char(0x00)) key.PartyVInfo.remove(0, 1);
				}
				// EncryptedData/KeyInfo/EncryptedKey/KeyInfo/AgreementMethod/KeyDerivationMethod/ConcatKDFParams/DigestMethod
				else if(xml.name() == "DigestMethod")
					key.concatDigest = xml.attributes().value("Algorithm").toString();
				// EncryptedData/KeyInfo/EncryptedKey/KeyInfo/AgreementMethod/OriginatorKeyInfo/KeyValue/ECKeyValue/PublicKey
				else if(xml.name() == "PublicKey")
				{
					xml.readNext();
					key.publicKey = fromBase64(xml.text());
				}
				// EncryptedData/KeyInfo/EncryptedKey/KeyInfo/X509Data/X509Certificate
				else if(xml.name() == "X509Certificate")
				{
					xml.readNext();
					key.cert = QSslCertificate( fromBase64( xml.text() ), QSsl::Der );
				}
				// EncryptedData/KeyInfo/EncryptedKey/KeyInfo/CipherData/CipherValue
				else if(xml.name() == "CipherValue")
				{
					xml.readNext();
					key.cipher = fromBase64(xml.text());
				}
			}
			keys << key;
		}
	}
	return QByteArray();
}

void CryptoDocPrivate::writeCDoc(QIODevice *cdoc, const QByteArray &transportKey,
	const QByteArray &encryptedData, const QString &file, const QString &ver, const QString &mime)
{
#ifndef NDEBUG
	qDebug() << "ENC Transport Key" << transportKey.toHex();
#endif

	qCDebug(CRYPTO) << "Writing CDOC file ver" << ver << "mime" << mime;
	QMultiHash<QString,QString> props;
	props.insert("DocumentFormat", "ENCDOC-XML|" + ver);
	props.insert("LibraryVersion", qApp->applicationName() + "|" + qApp->applicationVersion());
	props.insert("Filename", file);
	QList<File> reverse = files;
	std::reverse(reverse.begin(), reverse.end());
	for(const File &file: qAsConst(reverse))
		props.insert("orig_file", QString("%1|%2|%3|%4").arg(file.name).arg(file.data.size()).arg(file.mime).arg(file.id));

	QXmlStreamWriter w(cdoc);
	w.setAutoFormatting(true);
	w.writeStartDocument();
	w.writeNamespace(DENC, "denc");
	writeElement(w, DENC, "EncryptedData", [&]{
		if(!mime.isEmpty())
			w.writeAttribute("MimeType", mime);
		writeElement(w, DENC, "EncryptionMethod", {{"Algorithm", method}});
		w.writeNamespace(DS, "ds");
		writeElement(w, DS, "KeyInfo", [&]{
		for(const CKey &k: qAsConst(keys))
		{
			writeElement(w, DENC, "EncryptedKey", [&]{
				if(!k.id.isEmpty())
					w.writeAttribute("Id", k.id);
				if(!k.recipient.isEmpty())
					w.writeAttribute("Recipient", k.recipient);
				QByteArray cipher;
				if (k.cert.publicKey().algorithm() == QSsl::Rsa)
				{
					RSA *rsa = static_cast<RSA*>(k.cert.publicKey().handle());
					cipher.resize(RSA_size(rsa));
					if(opensslError(RSA_public_encrypt(transportKey.size(), pcuchar(transportKey.constData()),
						puchar(cipher.data()), rsa, RSA_PKCS1_PADDING) <= 0))
						return;
					writeElement(w, DENC, "EncryptionMethod", {{"Algorithm", RSA_MTH}});
					writeElement(w, DS, "KeyInfo", [&]{
						if(!k.name.isEmpty())
							w.writeTextElement(DS, "KeyName", k.name);
						writeElement(w, DS, "X509Data", [&]{
							writeBase64Element(w, DS, "X509Certificate", k.cert.toDer());
						});
					});
				}
				else
				{
					QByteArray derCert = k.cert.toDer();
					pcuchar pp = pcuchar(derCert.data());
					SCOPE(X509, peerCert, d2i_X509(nullptr, &pp, derCert.size()));
					SCOPE(EVP_PKEY, peerPKey, X509_get_pubkey(peerCert.get()));
					SCOPE(EC_KEY, peerECKey, EVP_PKEY_get1_EC_KEY(peerPKey.get()));
					int curve = EC_GROUP_get_curve_name(EC_KEY_get0_group(peerECKey.get()));
					SCOPE(EC_KEY, priv, EC_KEY_new_by_curve_name(curve));
					SCOPE(EVP_PKEY, pkey, EVP_PKEY_new());
					if (opensslError(EC_KEY_generate_key(priv.get()) <= 0) ||
						opensslError(EVP_PKEY_set1_EC_KEY(pkey.get(), priv.get()) <= 0))
						return;
					SCOPE(EVP_PKEY_CTX, ctx, EVP_PKEY_CTX_new(pkey.get(), nullptr));
					size_t sharedSecretLen = 0;
					if (opensslError(!ctx) ||
						opensslError(EVP_PKEY_derive_init(ctx.get()) <= 0) ||
						opensslError(EVP_PKEY_derive_set_peer(ctx.get(), peerPKey.get()) <= 0) ||
						opensslError(EVP_PKEY_derive(ctx.get(), nullptr, &sharedSecretLen) <= 0))
						return;
					QByteArray sharedSecret(int(sharedSecretLen), 0);
					if(opensslError(EVP_PKEY_derive(ctx.get(), puchar(sharedSecret.data()), &sharedSecretLen) <= 0))
						return;

					QByteArray oid(50, 0);
					oid.resize(OBJ_obj2txt(oid.data(), oid.size(), OBJ_nid2obj(EC_GROUP_get_curve_name(EC_KEY_get0_group(priv.get()))), 1));
					QByteArray SsDer(i2d_PublicKey(pkey.get(), nullptr), 0);
					puchar p = puchar(SsDer.data());
					i2d_PublicKey(pkey.get(), &p);

					const QString encryptionMethod = KWAES256_MTH;
					QString concatDigest = SHA384_MTH;
					switch((SsDer.size() - 1) / 2) {
					case 32: concatDigest = SHA256_MTH; break;
					case 48: concatDigest = SHA384_MTH; break;
					default: concatDigest = SHA512_MTH; break;
					}
					QByteArray encryptionKey = CryptoDoc::concatKDF(CryptoDocPrivate::SHA_MTH[concatDigest], KWAES_SIZE[encryptionMethod],
						sharedSecret, props.value("DocumentFormat").toUtf8() + SsDer + k.cert.toDer());
#ifndef NDEBUG
					qDebug() << "ENC Ss" << SsDer.toHex();
					qDebug() << "ENC Ksr" << sharedSecret.toHex();
					qDebug() << "ENC ConcatKDF" << encryptionKey.toHex();
#endif

					cipher = AES_wrap(encryptionKey, transportKey, true);
					if(opensslError(cipher.isEmpty()))
						return;

					writeElement(w, DENC, "EncryptionMethod", {{"Algorithm", encryptionMethod}});
					writeElement(w, DS, "KeyInfo", [&]{
						writeElement(w, DENC, "AgreementMethod", {{"Algorithm", AGREEMENT_MTH}}, [&]{
							w.writeNamespace(XENC11, "xenc11");
							writeElement(w, XENC11, "KeyDerivationMethod", {{"Algorithm", CONCATKDF_MTH}}, [&]{
								writeElement(w, XENC11, "ConcatKDFParams", {{"AlgorithmID", "00" + props.value("DocumentFormat").toUtf8().toHex()},
									{"PartyUInfo", "00" + SsDer.toHex()}, {"PartyVInfo", "00" + k.cert.toDer().toHex()}
								}, [&]{
									writeElement(w, DS, "DigestMethod", {{"Algorithm", concatDigest}});
								});
							});
							writeElement(w, DENC, "OriginatorKeyInfo", [&]{
								writeElement(w, DS, "KeyValue", [&]{
									w.writeNamespace(DSIG11, "dsig11");
									writeElement(w, DSIG11, "ECKeyValue", [&]{
										writeElement(w, DSIG11, "NamedCurve", {{"URI", "urn:oid:" + oid}});
										writeBase64Element(w, DSIG11, "PublicKey", SsDer);
									});
								});
							});
							writeElement(w, DENC, "RecipientKeyInfo", [&]{
								writeElement(w, DS, "X509Data", [&]{
									writeBase64Element(w, DS, "X509Certificate", k.cert.toDer());
								});
							});
						});
					});
				}
				writeElement(w, DENC, "CipherData", [&]{
					writeBase64Element(w, DENC, "CipherValue", cipher);
				});
			});
		}});
		writeElement(w,DENC, "CipherData", [&]{
			writeBase64Element(w, DENC, "CipherValue", encryptedData);
		});
		writeElement(w, DENC, "EncryptionProperties", [&]{
			for(QHash<QString,QString>::const_iterator i = props.constBegin(); i != props.constEnd(); ++i)
				writeElement(w, DENC, "EncryptionProperty", {{"Name", i.key()}}, [&]{ w.writeCharacters(i.value()); });
		});
	});
	w.writeEndDocument();
}

void CryptoDocPrivate::readDDoc(QIODevice *ddoc)
{
	qCDebug(CRYPTO) << "Parsing DDOC container";
	files.clear();
	QXmlStreamReader x(ddoc);
	while(!x.atEnd())
	{
		if(!x.readNextStartElement())
			continue;
		if(x.name() == "DataFile")
		{
			File file;
			file.name = x.attributes().value("Filename").toString().normalized(QString::NormalizationForm_C);
			file.id = x.attributes().value("Id").toString().normalized(QString::NormalizationForm_C);
			file.mime = x.attributes().value("MimeType").toString().normalized(QString::NormalizationForm_C);
			x.readNext();
			file.data = fromBase64( x.text() );
			file.size = FileDialog::fileSize(quint64(file.data.size()));
			files << file;
		}
		else if(x.name() == "Signature")
			hasSignature = true;
	}
	qCDebug(CRYPTO) << "Container contains signature" << hasSignature;
}

void CryptoDocPrivate::writeDDoc(QIODevice *ddoc)
{
	qCDebug(CRYPTO) << "Creating DDOC container";
	QXmlStreamWriter x(ddoc);
	x.setAutoFormatting(true);
	x.writeStartDocument();
	x.writeDefaultNamespace("http://www.sk.ee/DigiDoc/v1.3.0#");
	x.writeStartElement("SignedDoc");
	writeAttributes(x, {{"format", "DIGIDOC-XML"}, {"version", "1.3"}});

	for(const File &file: qAsConst(files))
	{
		x.writeStartElement("DataFile");
		writeAttributes(x, {{"ContentType", "EMBEDDED_BASE64"}, {"Filename", file.name},
			{"Id", file.id}, {"MimeType", file.mime}, {"Size", QString::number(file.data.size())}});
		x.writeDefaultNamespace("http://www.sk.ee/DigiDoc/v1.3.0#");
		writeBase64(x, file.data);
		x.writeEndElement(); //DataFile
	}

	x.writeEndElement(); //SignedDoc
	x.writeEndDocument();
}


CDocumentModel::CDocumentModel( CryptoDocPrivate *doc )
: d( doc )
{
	const_cast<QLoggingCategory&>(CRYPTO()).setEnabled( QtDebugMsg,
		QFile::exists( QString("%1/%2.log").arg( QDir::tempPath(), qApp->applicationName() ) ) );
}

void CDocumentModel::addFile(const QString &file, const QString &mime)
{
	if( d->isEncryptedWarning() )
		return;

	QString fileName(QFileInfo(file).fileName());
	for(auto containerFile: d->files)
	{
		qDebug() << containerFile.name << " vs " << file;
		if(containerFile.name == fileName)
		{
			d->setLastError(DocumentModel::tr("Cannot add the file to the envelope. File '%1' is already in container.").arg(fileName));
			return;
		}
	}

	QFile data(file);
	data.open(QFile::ReadOnly);
	CryptoDocPrivate::File f;
	f.id = QString("D%1").arg(d->files.size());
	f.mime = mime;
	f.name = QFileInfo(file).fileName();
	f.data = data.readAll();
	f.size = FileDialog::fileSize(quint64(f.data.size()));
	d->files << f;
	emit added(file);
}

void CDocumentModel::addTempFiles(const QStringList &files)
{
	for(auto file: files)
	{
		addFile(file);
		d->tempFiles << file;
	}
}

QString CDocumentModel::copy(int row, const QString &dst) const
{
	const CryptoDocPrivate::File &file = d->files.at(row);
	if( QFile::exists( dst ) )
		QFile::remove( dst );

	QFile f(dst);
	if(!f.open(QFile::WriteOnly) || f.write(file.data) < 0)
	{
		d->setLastError( tr("Failed to save file '%1'").arg( dst ) );
		return QString();
	}
	return dst;
}

QString CDocumentModel::data(int row) const
{
	const CryptoDocPrivate::File &f = d->files.at(row);
	if( f.name.isEmpty() )
		return QString();
	return f.name;
}

QString CDocumentModel::mime(int row) const
{
	const CryptoDocPrivate::File &f = d->files.at(row);
	if( f.mime.isEmpty() )
		return QString();
	return f.mime;
}

void CDocumentModel::open(int row)
{
	if(d->encrypted)
		return;
	QFileInfo f(copy(row, FileDialog::tempPath(data(row))));
	if( !f.exists() )
		return;
	d->tempFiles << f.absoluteFilePath();
#if defined(Q_OS_WIN)
	QStringList exts = QProcessEnvironment::systemEnvironment().value( "PATHEXT" ).split(';');
	exts << ".PIF" << ".SCR";
	if( exts.contains( "." + f.suffix(), Qt::CaseInsensitive ) &&
		QMessageBox::warning( qApp->activeWindow(), tr("DigiDoc4 client"),
			tr("This is an executable file! "
				"Executable files may contain viruses or other malicious code that could harm your computer. "
				"Are you sure you want to launch this file?"),
			QMessageBox::Yes|QMessageBox::No, QMessageBox::No ) == QMessageBox::No )
		return;
#else
	QFile::setPermissions( f.absoluteFilePath(), QFile::Permissions(0x6000) );
#endif
	emit openFile(f.absoluteFilePath());
}

bool CDocumentModel::removeRows(int row, int count)
{
	if(d->isEncryptedWarning())
		return false;

	if( d->files.isEmpty() || row >= d->files.size() )
	{
		d->setLastError( tr("Internal error") );
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
	if(d->encrypted)
		return QString();

	QString fileName = copy(row, path);
	QFileInfo f(fileName);
	if(f.exists())
		return fileName;

	return QString();
}



void CKey::setCert( const QSslCertificate &c )
{
	cert = c;
	recipient = SslCertificate(c).friendlyName();
}



CryptoDoc::CryptoDoc( QObject *parent )
:	QObject( parent )
,	d( new CryptoDocPrivate )
,	containerState(UnencryptedContainer)
{
	d->documents = new CDocumentModel( d );
}

CryptoDoc::~CryptoDoc() { clear(); delete d; }

bool CryptoDoc::addKey( const CKey &key )
{
	if( d->isEncryptedWarning() )
		return false;
	if( d->keys.contains( key ) )
	{
		d->setLastError( tr("Key already exists") );
		return false;
	}
	d->keys << key;
	return true;
}

bool CryptoDoc::canDecrypt(const QSslCertificate &cert)
{
	for(const CKey &k: qAsConst(d->keys))
	{
		if(!d->ENC_MTH.contains(d->method) || k.cert != cert)
			continue;
		if(cert.publicKey().algorithm() == QSsl::Rsa &&
				!k.cipher.isEmpty() &&
				k.method == CryptoDocPrivate::RSA_MTH)
			return true;
		if(cert.publicKey().algorithm() == QSsl::Ec &&
				!k.publicKey.isEmpty() &&
				!k.cipher.isEmpty() &&
				d->KWAES_SIZE.contains(k.method) &&
				k.derive == CryptoDocPrivate::CONCATKDF_MTH &&
				k.agreement == CryptoDocPrivate::AGREEMENT_MTH)
			return true;
	}
	return false;
}

QByteArray CryptoDoc::concatKDF(QCryptographicHash::Algorithm hashAlg, quint32 keyDataLen, const QByteArray &z, const QByteArray &otherInfo)
{
	quint32 hashLen = 0;
	switch(hashAlg)
	{
	case QCryptographicHash::Sha256: hashLen = 32; break;
	case QCryptographicHash::Sha384: hashLen = 48; break;
	case QCryptographicHash::Sha512: hashLen = 64; break;
	default: return QByteArray();
	}
	quint32 reps = quint32(std::ceil(double(keyDataLen) / double(hashLen)));
	QCryptographicHash md(hashAlg);
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
	delete d->ddoc;
	for(const QString &file: qAsConst(d->tempFiles))
		QFile::remove(file);
	d->tempFiles.clear();
	d->ddoc = nullptr;
	d->hasSignature = false;
	d->encrypted = false;
	d->fileName = file;
	d->files.clear();
	d->keys.clear();
	d->properties.clear();
	d->method.clear();
	d->mime.clear();
}

ContainerState CryptoDoc::state()
{
	return containerState;
}

bool CryptoDoc::decrypt()
{
	if( d->fileName.isEmpty() )
	{
		d->setLastError( tr("Container is not open") );
		return false;
	}
	if( !d->encrypted )
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
		d->setLastError( tr("You do not have the key to decrypt this document") );
		return false;
	}

	bool decrypted = false;
	bool isECDH = qApp->signer()->tokenauth().cert().publicKey().algorithm() == QSsl::Ec;
	QByteArray decryptedKey;
	while( !decrypted )
	{
		switch(qApp->signer()->decrypt(isECDH ? key.publicKey : key.cipher, decryptedKey,
			key.concatDigest, d->KWAES_SIZE[key.method], key.AlgorithmID, key.PartyUInfo, key.PartyVInfo))
		{
		case QSigner::DecryptOK: decrypted = true; break;
		case QSigner::PinIncorrect: break;
		default: return false;
		}
	}
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
		d->setLastError( d->lastError );
	
	containerState = d->encrypted ? EncryptedContainer : DecryptedContainer;

	return !d->encrypted;
}

DocumentModel* CryptoDoc::documentModel() const { return d->documents; }

bool CryptoDoc::encrypt( const QString &filename )
{
	if( !filename.isEmpty() )
		d->fileName = filename;
	if( d->fileName.isEmpty() )
	{
		d->setLastError( tr("Container is not open") );
		return false;
	}
	if( d->encrypted )
		return true;
	if( d->keys.isEmpty() )
	{
		d->setLastError( tr("No keys specified") );
		return false;
	}

	d->waitForFinished();
	if( !d->lastError.isEmpty() )
		d->setLastError( d->lastError );
	open(d->fileName);

	containerState = d->encrypted ? EncryptedContainer : DecryptedContainer;

	return d->encrypted;
}

QString CryptoDoc::fileName() const { return d->fileName; }
bool CryptoDoc::isEncrypted() const { return d->encrypted; }
bool CryptoDoc::isNull() const { return d->fileName.isEmpty(); }
bool CryptoDoc::isSigned() const { return d->hasSignature; }

QList<CKey> CryptoDoc::keys()
{
	return d->keys;
}

QList<QString> CryptoDoc::files()
{
	QList<QString> fileList;

	for(CryptoDocPrivate::File f: d->files)
	{
		fileList << f.name;
	}

	return fileList;
}

bool CryptoDoc::move(const QString &to)
{
	if(containerState == ContainerState::UnencryptedContainer)
	{
		d->fileName = to;
		return true;
	}

	return false;
}

bool CryptoDoc::open( const QString &file )
{
	clear(file);
	QFile cdoc(d->fileName);
	cdoc.open(QFile::ReadOnly);
	d->readCDoc(&cdoc, false);
	cdoc.close();

	if(d->files.isEmpty() && d->properties.contains("Filename"))
	{
		CryptoDocPrivate::File f;
		f.name = d->properties["Filename"];
		f.mime = d->mime == CryptoDocPrivate::MIME_ZLIB ? d->properties["OriginalMimeType"] : d->mime;
		f.size = d->size(d->properties["OriginalSize"]);
		d->files << f;
	}

	d->encrypted = true;
	containerState = EncryptedContainer;
	qApp->addRecent( file );
	return !d->keys.isEmpty();
}

void CryptoDoc::removeKey( int id )
{
	if( !d->isEncryptedWarning() )
		d->keys.removeAt(id);
}

bool CryptoDoc::saveDDoc( const QString &filename )
{
	if( !d->ddoc )
	{
		d->setLastError( tr("Document not open") );
		return false;
	}

	bool result = d->ddoc->copy( filename );
	if( !result )
		d->setLastError( tr("Failed to save file") );
	return result;
}

#include "CryptoDoc.moc"
