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
#include <QtCore/QDir>
#include <QtCore/QFileInfo>
#include <QtCore/QLoggingCategory>
#include <QtCore/QMimeData>
#include <QtCore/QProcessEnvironment>
#include <QtCore/QTemporaryFile>
#include <QtCore/QThread>
#include <QtCore/QUrl>
#include <QtCore/QXmlStreamReader>
#include <QtCore/QXmlStreamWriter>
#include <QtGui/QDesktopServices>
#include <QtNetwork/QSslKey>
#include <QtWidgets/QMessageBox>

#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/rand.h>
#include <openssl/rsa.h>

#include <memory>

using namespace ria::qdigidoc4;

#define MIME_XML  "text/xml"
#define MIME_ZLIB "http://www.isi.edu/in-noes/iana/assignments/media-types/application/zip"
#define MIME_DDOC "http://www.sk.ee/DigiDoc/v1.3.0/digidoc.xsd"
#define MIME_DDOC_OLD "http://www.sk.ee/DigiDoc/1.3.0/digidoc.xsd"

Q_LOGGING_CATEGORY(CRYPTO,"CRYPTO")

class CryptoDocPrivate: public QThread
{
	Q_OBJECT
public:
	struct File
	{
		QString name, id, mime, size;
		QByteArray data;
	};

	CryptoDocPrivate(): hasSignature(false), encrypted(false), documents(nullptr), ddoc(nullptr) {}

	QByteArray crypto(const QByteArray &iv, const QByteArray &key, const QByteArray &data, bool encrypt) const;
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
		connect( this, SIGNAL(finished()), &e, SLOT(quit()) );
		start();
		e.exec();
	}
	inline void writeBase64(QXmlStreamWriter &x, const QString &ns, const QString &name, const QByteArray &data)
	{
		x.writeStartElement(ns, name);
		for(int i = 0; i < data.size(); i+=48)
			x.writeCharacters(data.mid(i, 48).toBase64() + "\n");
		x.writeEndElement();
	}
	void writeCDoc(QIODevice *cdoc, const QByteArray &key, const QByteArray &data, const QString &file, const QString &ver, const QString &mime);
	void writeDDoc(QIODevice *ddoc);

	QString			method, mime, fileName, lastError;
	QByteArray		key;
	QHash<QString,QString> properties;
	QList<CKey>		keys;
	QList<File>		files;
	bool			hasSignature, encrypted;
	CDocumentModel	*documents;
	QTemporaryFile	*ddoc;
	QStringList		tempFiles;
};

QByteArray CryptoDocPrivate::crypto( const QByteArray &iv, const QByteArray &key, const QByteArray &data, bool encrypt ) const
{
	qCDebug(CRYPTO) << "Encrypt" << encrypt <<  "IV" << iv.toHex() << "KEY" << key.toHex();

	int size = 0, size2 = 0;
	std::unique_ptr<EVP_CIPHER_CTX,decltype(&EVP_CIPHER_CTX_free)> ctx(EVP_CIPHER_CTX_new(), EVP_CIPHER_CTX_free);
	int err = EVP_CipherInit(ctx.get(), EVP_aes_128_cbc(), (unsigned char*)key.constData(), (unsigned char*)iv.constData(), encrypt);
	if(opensslError(err == 0))
		return QByteArray();

	QByteArray result(data.size() + EVP_CIPHER_CTX_block_size(ctx.get()), Qt::Uninitialized);
	unsigned char *resultPointer = (unsigned char*)result.data(); //Detach only once
	err = EVP_CipherUpdate(ctx.get(), resultPointer, &size, (const unsigned char*)data.constData(), data.size());
	if(opensslError(err == 0))
		return QByteArray();

	err = EVP_CipherFinal(ctx.get(), resultPointer + size, &size2);
	if(opensslError(err == 0))
		return QByteArray();
	result.resize(size + size2);
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

		buf = (buf << 6) | d;
		nbits += 6;
		if(nbits >= 8)
		{
			nbits -= 8;
			result[offset++] = buf >> nbits;
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
		unsigned long errorCode;
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

		// add ANSIX923 padding
		QByteArray ansix923(16 - (data.size() % 16), 0);
		qCDebug(CRYPTO) << "Adding ANSIX923 padding size" << ansix923.size();
		ansix923[ansix923.size() - 1] = ansix923.size();
		data.write(ansix923);
		data.close();

#ifdef WIN32
		RAND_screen();
#else
		RAND_load_file("/dev/urandom", 1024);
#endif
		unsigned char salt[PKCS5_SALT_LEN], indata[128];
		RAND_bytes(salt, sizeof(salt));
		RAND_bytes(indata, sizeof(indata));

		QByteArray iv(EVP_MAX_IV_LENGTH, 0), key(16, 0);
		int err = EVP_BytesToKey(EVP_aes_128_cbc(), EVP_md5(), salt, indata, sizeof(indata),
			1, (unsigned char*)key.data(), (unsigned char*)iv.data());
		if(opensslError(err == 0))
			return;
		QByteArray result = crypto(iv, key, data.data(), true);
		result.prepend(iv);

		QFile cdoc(fileName);
		cdoc.open(QFile::WriteOnly);
		writeCDoc(&cdoc, key, result, name, "1.0", mime);
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

		result = crypto(result.left(16), key, result.mid(16), false);

		// remove ANSIX923 padding
		if(result.size() > 0)
		{
			QByteArray ansix923(result[result.size()-1], 0);
			ansix923[ansix923.size()-1] = ansix923.size();
			if(result.right(ansix923.size()) == ansix923)
			{
				qCDebug(CRYPTO) << "Removing ANSIX923 padding size:" << ansix923.size();
				result.resize(result.size() - ansix923.size());
			}
		}

		if(mime == MIME_ZLIB)
		{
			// Add size header for qUncompress compatibilty
			unsigned int origsize = std::max<int>(properties["OriginalSize"].toUInt(), 1);
			qCDebug(CRYPTO) << "Decompressing zlib content size" << origsize;
			QByteArray size(4, 0);
			size[0] = (origsize & 0xff000000) >> 24;
			size[1] = (origsize & 0x00ff0000) >> 16;
			size[2] = (origsize & 0x0000ff00) >> 8;
			size[3] = (origsize & 0x000000ff);
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
				f.size = FileDialog::fileSize(result.size());
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
					key.chipher = fromBase64( xml.text() );
				}
			}
			keys << key;
		}
	}
	return QByteArray();
}

void CryptoDocPrivate::writeCDoc(QIODevice *cdoc, const QByteArray &key, const QByteArray &data, const QString &file, const QString &ver, const QString &mime)
{
	qCDebug(CRYPTO) << "Writing CDOC file, key" << key.toHex() << "ver" << ver << "mime" << mime;
	QHash<QString,QString> props;
	props["DocumentFormat"] = "ENCDOC-XML|" + ver;
	props["LibraryVersion"] = qApp->applicationName() + "|" + qApp->applicationVersion();
	props["Filename"] = file;

	static const QString DS = "http://www.w3.org/2000/09/xmldsig#";
	static const QString DENC = "http://www.w3.org/2001/04/xmlenc#";

	QXmlStreamWriter w(cdoc);
	w.setAutoFormatting(true);
	w.writeStartDocument();
	w.writeNamespace(DENC, "denc");
	w.writeStartElement(DENC, "EncryptedData");
	if(!mime.isEmpty())
		w.writeAttribute("MimeType", mime);

	w.writeStartElement(DENC, "EncryptionMethod");
	w.writeAttribute("Algorithm", "http://www.w3.org/2001/04/xmlenc#aes128-cbc");
	w.writeEndElement(); //EncryptionMethod

	w.writeNamespace(DS, "ds");
	w.writeStartElement(DS, "KeyInfo");
	for(const CKey &k: keys)
	{
		w.writeStartElement(DENC, "EncryptedKey");
		if(!k.id.isEmpty())
			w.writeAttribute("Id", k.id);
		if(!k.recipient.isEmpty())
			w.writeAttribute("Recipient", k.recipient);

		w.writeStartElement(DENC, "EncryptionMethod");
		w.writeAttribute("Algorithm", "http://www.w3.org/2001/04/xmlenc#rsa-1_5");
		w.writeEndElement(); //EncryptionMethod

		w.writeStartElement(DS, "KeyInfo");
		if(!k.name.isEmpty())
			w.writeTextElement(DS, "KeyName", k.name);
		w.writeStartElement(DS, "X509Data");
		writeBase64(w, DS, "X509Certificate", k.cert.toDer());
		w.writeEndElement(); //X509Data
		w.writeEndElement(); //KeyInfo
		w.writeStartElement(DENC, "CipherData");

		RSA *rsa = (RSA*)k.cert.publicKey().handle();
		QByteArray chipper(RSA_size(rsa), 0);
		int err = RSA_public_encrypt(key.size(), (unsigned char*)key.constData(), (unsigned char*)chipper.data(), rsa, RSA_PKCS1_PADDING);
		opensslError(err != 1);
		writeBase64(w, DENC, "CipherValue", chipper);
		w.writeEndElement(); //CipherData
		w.writeEndElement(); //EncryptedKey
	}
	w.writeEndElement(); //KeyInfo

	w.writeStartElement(DENC, "CipherData");
	writeBase64(w, DENC, "CipherValue", data);
	w.writeEndElement(); //CipherData

	w.writeStartElement(DENC, "EncryptionProperties");
	for(QHash<QString,QString>::const_iterator i = props.constBegin(); i != props.constEnd(); ++i)
	{
		w.writeStartElement(DENC, "EncryptionProperty");
		w.writeAttribute("Name", i.key());
		w.writeCharacters(i.value());
		w.writeEndElement(); //EncryptionProperty
	}
	for(const File &file: files)
	{
		w.writeStartElement(DENC, "EncryptionProperty");
		w.writeAttribute("Name", "orig_file");
		w.writeCharacters(QString("%1|%2|%3|%4").arg(file.name).arg(file.data.size()).arg(file.mime).arg(file.id));
		w.writeEndElement(); //EncryptionProperty
	}
	w.writeEndElement(); //EncryptionProperties
	w.writeEndElement(); //EncryptedData
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
			file.size = FileDialog::fileSize(file.data.size());
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
	x.writeAttribute("format", "DIGIDOC-XML");
	x.writeAttribute("version", "1.3");

	for(const File &file: files)
	{
		x.writeStartElement("DataFile");
		x.writeAttribute("ContentType", "EMBEDDED_BASE64");
		x.writeAttribute("Filename", file.name);
		x.writeAttribute("Id", file.id);
		x.writeAttribute("MimeType", file.mime);
		x.writeAttribute("Size", QString::number(file.data.size()));
		x.writeDefaultNamespace("http://www.sk.ee/DigiDoc/v1.3.0#");
		for(int i = 0; i < file.data.size(); i+=48)
			x.writeCharacters(file.data.mid(i, 48).toBase64() + "\n");
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
			d->setLastError(QString("File '%1' already in container").arg(file));
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
	f.size = FileDialog::fileSize(f.data.size());
	d->files << f;
	emit added(file);
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
		QMessageBox::warning( qApp->activeWindow(), tr("DigiDoc3 crypto"),
			tr("This is an executable file! "
				"Executable files may contain viruses or other malicious code that could harm your computer. "
				"Are you sure you want to launch this file?"),
			QMessageBox::Yes|QMessageBox::No, QMessageBox::No ) == QMessageBox::No )
		return;
#else
	QFile::setPermissions( f.absoluteFilePath(), QFile::Permissions(0x6000) );
#endif
	QDesktopServices::openUrl( QUrl::fromLocalFile( f.absoluteFilePath() ) );
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
		d->files.removeAt( i );
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

void CryptoDoc::clear( const QString &file )
{
	delete d->ddoc;
	for(const QString &file: d->tempFiles)
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
	for(const CKey &k: d->keys)
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
	while( !decrypted )
	{
		switch( qApp->signer()->decrypt( key.chipher, d->key ) )
		{
		case QSigner::DecryptOK: decrypted = true; break;
		case QSigner::PinIncorrect: break;
		default: return false;
		}
	}

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
		f.mime = d->mime == MIME_ZLIB ? d->properties["OriginalMimeType"] : d->mime;
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
