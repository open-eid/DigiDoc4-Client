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
#include "CDoc1.h"
#include "CDoc2.h"
#include "Crypto.h"
#include "TokenData.h"
#include "QCryptoBackend.h"
#include "QSigner.h"
#include "Settings.h"
#include "SslCertificate.h"
#include "Utils.h"
#include "dialogs/FileDialog.h"
#include "dialogs/WarningDialog.h"

#include <QtCore/QDebug>
#include <QtCore/QDir>
#include <QtCore/QFileInfo>
#include <QtCore/QRegularExpression>
#include <QtCore/QThread>
#include <QtCore/QUrl>
#include <QtCore/QUrlQuery>
#include <QtGui/QDesktopServices>
#include <QtNetwork/QSslKey>
#include <QtWidgets/QMessageBox>

using namespace ria::qdigidoc4;

class CryptoDoc::Private final: public QThread
{
	Q_OBJECT
public:
	bool isEncryptedWarning(const QString &title) const;
	void run() final;
	inline void waitForFinished()
	{
		QEventLoop e;
		connect(this, &Private::finished, &e, &QEventLoop::quit);
		start();
		e.exec();
	}

	std::unique_ptr<CDoc> cdoc;
	QString			fileName;
	QByteArray		key;
	bool			isEncrypted = false;
	CDocumentModel	*documents = new CDocumentModel(this);
	QStringList		tempFiles;
};

bool CryptoDoc::Private::isEncryptedWarning(const QString &title) const
{
	if( fileName.isEmpty() )
		WarningDialog::create()->withTitle(title)->withText(CryptoDoc::tr("Container is not open"))->open();
	if(isEncrypted)
		WarningDialog::create()->withTitle(title)->withText(CryptoDoc::tr("Container is encrypted"))->open();
	return fileName.isEmpty() || isEncrypted;
}

void CryptoDoc::Private::run()
{
	if(isEncrypted)
	{
		qCDebug(CRYPTO) << "Decrypt" << fileName;
		isEncrypted = !cdoc->decryptPayload(key);
	}
	else
	{
		qCDebug(CRYPTO) << "Encrypt" << fileName;
		isEncrypted = cdoc->save(fileName);
	}
}

CDocumentModel::CDocumentModel(CryptoDoc::Private *doc)
: d( doc )
{}

bool CDocumentModel::addFile(const QString &file, const QString &mime)
{
	if(d->isEncryptedWarning(DocumentModel::tr("Failed to add file")))
		return false;

	QFileInfo info(file);
	if(!addFileCheck(d->fileName, info))
		return false;

	if(d->cdoc->version() == 1 && info.size() > 120*1024*1024)
	{
		WarningDialog::create()
			->withTitle(DocumentModel::tr("Failed to add file"))
			->withText(tr("Added file(s) exceeds the maximum size limit of the container (∼120MB). "
				"<a href='https://www.id.ee/en/article/encrypting-large-120-mb-files/'>Read more about it</a>"))
			->open();
		return false;
	}

	auto data = std::make_unique<QFile>(file);
	if(!data->open(QFile::ReadOnly))
		return false;
	d->cdoc->files.push_back({
		QFileInfo(file).fileName(),
		QStringLiteral("D%1").arg(d->cdoc->files.size()),
		mime,
		data->size(),
		std::move(data),
	});
	emit added(FileDialog::normalized(d->cdoc->files.back().name));
	return true;
}

void CDocumentModel::addTempReference(const QString &file)
{
	d->tempFiles.append(file);
}

QString CDocumentModel::copy(int row, const QString &dst) const
{
	const CDoc::File &file = d->cdoc->files.at(row);
	if( QFile::exists( dst ) )
		QFile::remove( dst );
	file.data->seek(0);
	if(QFile f(dst); f.open(QFile::WriteOnly) && copyIODevice(file.data.get(), &f) == file.size)
		return dst;
	WarningDialog::create()
		->withTitle(FileDialog::tr("Failed to save file"))
		->withText(dst)
		->open();
	return {};
}

QString CDocumentModel::data(int row) const
{
	return FileDialog::normalized(d->cdoc->files.at(row).name);
}

quint64 CDocumentModel::fileSize(int row) const
{
	return d->cdoc->files.at(row).size;
}

QString CDocumentModel::mime(int row) const
{
	return FileDialog::normalized(d->cdoc->files.at(row).mime);
}

void CDocumentModel::open(int row)
{
	if(d->isEncrypted)
		return;
	QString path = FileDialog::tempPath(FileDialog::safeName(data(row)));
	if(!verifyFile(path))
		return;
	if(copy(row, path).isEmpty())
		return;
	d->tempFiles.append(path);
	FileDialog::setReadOnly(path);
	if(FileDialog::isSignedPDF(path))
		Application::showClient({ std::move(path) }, false, false, true);
	else
		QDesktopServices::openUrl(QUrl::fromLocalFile(path));
}

bool CDocumentModel::removeRow(int row)
{
	if(d->isEncryptedWarning(DocumentModel::tr("Failed remove document from container")))
		return false;

	if(d->cdoc->files.empty() || row >= d->cdoc->files.size())
	{
		WarningDialog::create()
			->withTitle(DocumentModel::tr("Failed remove document from container"))
			->withText(DocumentModel::tr("Internal error"))
			->open();
		return false;
	}

	d->cdoc->files.erase(d->cdoc->files.cbegin() + row);
	return true;
}

int CDocumentModel::rowCount() const
{
	return int(d->cdoc->files.size());
}

QString CDocumentModel::save(int row, const QString &path) const
{
	if(d->isEncrypted)
		return {};

	QString fileName = copy(row, path);
	QFileInfo f(fileName);
	if(!f.exists())
		return {};
	FileDialog::setFileZone(fileName, d->fileName);
	return fileName;
}

CKey::CKey(Tag)
	: unsupported(true)
{}

CKey::CKey(const QSslCertificate &c)
{
	setCert(c);
	recipient = [](const SslCertificate &c) {
		QString cn = c.subjectInfo(QSslCertificate::CommonName);
		QString gn = c.subjectInfo("GN");
		QString sn = c.subjectInfo("SN");
		if(!gn.isEmpty() || !sn.isEmpty())
			cn = QStringLiteral("%1 %2 %3").arg(gn, sn, c.personalCode());

		int certType = c.type();
		if(certType & SslCertificate::EResidentSubType)
			return QStringLiteral("%1 %2").arg(cn, CryptoDoc::tr("Digi-ID E-RESIDENT"));
		if(certType & SslCertificate::DigiIDType)
			return QStringLiteral("%1 %2").arg(cn, CryptoDoc::tr("Digi-ID"));
		if(certType & SslCertificate::EstEidType)
			return QStringLiteral("%1 %2").arg(cn, CryptoDoc::tr("ID-CARD"));
		return cn;
	}(c);
}

void CKey::setCert(const QSslCertificate &c)
{
	QSslKey k = c.publicKey();
	cert = c;
	key = Crypto::toPublicKeyDer(k);
	isRSA = k.algorithm() == QSsl::Rsa;
}

QHash<QString, QString> CKey::fromKeyLabel() const
{
	QHash<QString,QString> result;
	if(!recipient.startsWith(QLatin1String("data:"), Qt::CaseInsensitive))
		return result;
	QString payload = recipient.mid(5);
	QString mimeType;
	QString encoding;
	if(auto pos = payload.indexOf(','); pos != -1)
	{
		mimeType = payload.left(pos);
		payload = payload.mid(pos + 1);
		if(auto header = mimeType.split(';'); header.size() == 2)
		{
			mimeType = header.value(0);
			encoding = header.value(1);
		}
	}
	if(!mimeType.isEmpty() && mimeType != QLatin1String("application/x-www-form-urlencoded"))
		return result;
	if(encoding == QLatin1String("base64"))
		payload = QByteArray::fromBase64(payload.toLatin1());
	;
	for(const auto &[key,value]: QUrlQuery(payload).queryItems(QUrl::FullyDecoded))
		result[key.toLower()] = value;
	if(!result.contains(QStringLiteral("type")) || !result.contains(QStringLiteral("v")))
		result.clear();
	return result;
}

QString CKey::toKeyLabel() const
{
	if(cert.isNull())
		return recipient;
	QDateTime exp = cert.expiryDate();
	if(Settings::CDOC2_USE_KEYSERVER)
		exp = std::min(exp, QDateTime::currentDateTimeUtc().addMonths(6));
	auto escape = [](QString data) { return data.replace(',', QLatin1String("%2C")); };
	QString type = QStringLiteral("ID-card");
	if(auto t = SslCertificate(cert).type(); t & SslCertificate::EResidentSubType)
		type = QStringLiteral("Digi-ID E-RESIDENT");
	else if(t & SslCertificate::DigiIDType)
		type = QStringLiteral("Digi-ID");
	QUrlQuery q;
	q.setQueryItems({
		{QStringLiteral("v"), QString::number(1)},
		{QStringLiteral("type"), type},
		{QStringLiteral("serial_number"), escape(cert.subjectInfo("serialNumber").join(','))},
		{QStringLiteral("cn"), escape(cert.subjectInfo("CN").join(','))},
		{QStringLiteral("server_exp"), QString::number(exp.toSecsSinceEpoch())},
	});
	return "data:" + q.query(QUrl::FullyEncoded);
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
	if(d->isEncryptedWarning(tr("Failed to add key")))
		return false;
	if(d->cdoc->keys.contains(key))
	{
		WarningDialog::create()
			->withTitle(tr("Failed to add key"))
			->withText(tr("Key already exists"))
			->open();
		return false;
	}
	d->cdoc->keys.append(key);
	return true;
}

bool CryptoDoc::canDecrypt(const QSslCertificate &cert)
{
	return !d->cdoc->canDecrypt(cert).key.isEmpty();
}

void CryptoDoc::clear( const QString &file )
{
	for(const QString &f: qAsConst(d->tempFiles))
	{
		//reset read-only attribute to enable delete file
		FileDialog::setReadOnly(f, false);
		QFile::remove(f);
	}
	d->tempFiles.clear();
	d->isEncrypted = false;
	d->fileName = file;
	if(Settings::CDOC2_DEFAULT)
		d->cdoc = std::make_unique<CDoc2>();
	else
		d->cdoc = std::make_unique<CDoc1>();
}

ContainerState CryptoDoc::state() const
{
	return d->isEncrypted ? EncryptedContainer : UnencryptedContainer;
}

bool CryptoDoc::decrypt()
{
	if( d->fileName.isEmpty() )
	{
		WarningDialog::create()
			->withTitle(QSigner::tr("Failed to decrypt document"))
			->withText(tr("Container is not open"))
			->open();
		return false;
	}
	if(!d->isEncrypted)
		return true;

	CKey key = d->cdoc->canDecrypt(qApp->signer()->tokenauth().cert());
	if(key.key.isEmpty())
	{
		WarningDialog::create()
			->withTitle(QSigner::tr("Failed to decrypt document"))
			->withText(tr("You do not have the key to decrypt this document"))
			->open();
		return false;
	}

	if(d->cdoc->version() == 2 && !key.transaction_id.isEmpty() && !Settings::CDOC2_NOTIFICATION.isSet())
	{
		auto *dlg = WarningDialog::create()
			->withTitle(QSigner::tr("Failed to decrypt document"))
			->withText(tr("You must enter your PIN code twice in order to decrypt the CDOC2 container. "
				"The first PIN entry is required for authentication to the key server referenced in the CDOC2 container. "
				"Second PIN entry is required to decrypt the CDOC2 container."))
			->setCancelText(WarningDialog::Cancel)
			->addButton(WarningDialog::OK, QMessageBox::Ok)
			->addButton(tr("Don't show again"), QMessageBox::Ignore);
		switch (dlg->exec())
		{
		case QMessageBox::Ok: break;
		case QMessageBox::Ignore:
			Settings::CDOC2_NOTIFICATION = true;
			break;
		default: return false;
		}
	}

	d->key = d->cdoc->transportKey(key);
#ifndef NDEBUG
	qDebug() << "Transport key" << d->key.toHex();
#endif
	if(d->key.isEmpty())
	{
		WarningDialog::create()
			->withTitle(QSigner::tr("Failed to decrypt document"))
			->withText(tr("Please check your internet connection and network settings."))
			->withDetails(d->cdoc->lastError)
			->open();
		return false;
	}

	d->waitForFinished();
	if(d->isEncrypted)
		WarningDialog::create()
			->withTitle(QSigner::tr("Failed to decrypt document"))
			->withText(tr("Error parsing document"))
			->open();
	if(!d->cdoc->lastError.isEmpty())
		WarningDialog::create()
			->withTitle(QSigner::tr("Failed to decrypt document"))
			->withDetails(d->cdoc->lastError)
			->open();

	return !d->isEncrypted;
}

DocumentModel* CryptoDoc::documentModel() const { return d->documents; }

bool CryptoDoc::encrypt( const QString &filename )
{
	if( !filename.isEmpty() )
		d->fileName = filename;
	if( d->fileName.isEmpty() )
	{
		WarningDialog::create()
			->withTitle(tr("Failed to encrypt document"))
			->withText(tr("Container is not open"))
			->open();
		return false;
	}
	if(d->isEncrypted)
		return true;
	if(d->cdoc->keys.isEmpty())
	{
		WarningDialog::create()
			->withTitle(tr("Failed to encrypt document"))
			->withText(tr("No keys specified"))
			->open();
		return false;
	}

	d->waitForFinished();
	if(d->isEncrypted)
		open(d->fileName);
	else
		WarningDialog::create()
			->withTitle(tr("Failed to encrypt document"))
			->withText(tr("Please check your internet connection and network settings."))
			->withDetails(d->cdoc->lastError)
			->open();
	return d->isEncrypted;
}

QString CryptoDoc::fileName() const { return d->fileName; }

QList<CKey> CryptoDoc::keys() const
{
	return d->cdoc->keys;
}

bool CryptoDoc::move(const QString &to)
{
	if(d->isEncrypted)
		return false;
	d->fileName = to;
	return true;
}

bool CryptoDoc::open( const QString &file )
{
	clear(file);
	d->cdoc = std::make_unique<CDoc2>(d->fileName);
	if(d->cdoc->keys.isEmpty())
		d->cdoc = std::make_unique<CDoc1>(d->fileName);
	d->isEncrypted = bool(d->cdoc);
	if(!d->isEncrypted || d->cdoc->keys.isEmpty())
	{
		WarningDialog::create()
			->withTitle(tr("Failed to open document"))
			->withDetails(d->cdoc->lastError)
			->open();
		return false;
	}
	Application::addRecent( file );
	return true;
}

void CryptoDoc::removeKey( int id )
{
	if(!d->isEncryptedWarning(tr("Failed to remove key")))
		d->cdoc->keys.removeAt(id);
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
