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
#include <QtGui/QDesktopServices>
#include <QtNetwork/QSslKey>
#include <QtWidgets/QMessageBox>

using namespace ria::qdigidoc4;

class CryptoDoc::Private final: public QThread
{
	Q_OBJECT
public:
	bool isEncryptedWarning() const;
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
	bool			isEncrypted = false;
	CDocumentModel	*documents = new CDocumentModel(this);
	QStringList		tempFiles;
	// Decryption data
	QByteArray fmk;
	// Encryption data
	QString label;
	QByteArray secret;
	uint32_t kdf_iter;
};

bool CryptoDoc::Private::isEncryptedWarning() const
{
	if( fileName.isEmpty() )
		WarningDialog::show(CryptoDoc::tr("Container is not open"));
	if(isEncrypted)
		WarningDialog::show(CryptoDoc::tr("Container is encrypted"));
	return fileName.isEmpty() || isEncrypted;
}

void CryptoDoc::Private::run()
{
	if(isEncrypted)
	{
		qCDebug(CRYPTO) << "Decrypt" << fileName;
		isEncrypted = !cdoc->decryptPayload(fmk);
	}
	else
	{
		qCDebug(CRYPTO) << "Encrypt" << fileName;
		if (secret.isEmpty()) {
			isEncrypted = cdoc->save(fileName);
		} else {
			isEncrypted = CDoc2::save(fileName, cdoc->files, label, secret, kdf_iter);
		}
	}
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
		WarningDialog::show(DocumentModel::tr("Cannot add empty file to the container."));
		return false;
	}
	if(d->cdoc->version() == 1 && info.size() > 120*1024*1024)
	{
		WarningDialog::show(tr("Added file(s) exceeds the maximum size limit of the container (∼120MB). "
							   "<a href='https://www.id.ee/en/article/encrypting-large-120-mb-files/'>Read more about it</a>"));
		return false;
	}

	QString fileName(info.fileName());
	if(std::any_of(d->cdoc->files.cbegin(), d->cdoc->files.cend(),
				   [&fileName](const auto &containerFile) { return containerFile.name == fileName; }))
	{
		WarningDialog::show(DocumentModel::tr("Cannot add the file to the envelope. File '%1' is already in container.")
							.arg(FileDialog::normalized(fileName)));
		return false;
	}

	auto data = std::make_unique<QFile>(file);
	data->open(QFile::ReadOnly);
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
	WarningDialog::show(tr("Failed to save file '%1'").arg(dst));
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
	if(d->isEncryptedWarning())
		return false;

	if(d->cdoc->files.empty() || row >= d->cdoc->files.size())
	{
		WarningDialog::show(DocumentModel::tr("Internal error"));
		return false;
	}

	d->cdoc->files.erase(d->cdoc->files.cbegin() + row);
	emit removed(row);
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

	int zone = FileDialog::fileZone(d->fileName);
	QString fileName = copy(row, path);
	QFileInfo f(fileName);
	if(!f.exists())
		return {};
	FileDialog::setFileZone(fileName, zone);
	return fileName;
}

bool
CKey::isTheSameRecipient(const CKey& other) const
{
	QByteArray this_key, other_key;
	if (this->isCertificate()) {
		const CKeyCert& ckc = static_cast<const CKeyCert&>(*this);
		this_key = ckc.cert.publicKey().toDer();
	}
	if (other.isCertificate()) {
		const CKeyCert& ckc = static_cast<const CKeyCert&>(other);
		other_key = ckc.cert.publicKey().toDer();
	}
	if (this_key.isEmpty() || other_key.isEmpty()) return false;
	return this_key == other_key;
}

bool
CKey::isTheSameRecipient(const QSslCertificate &cert) const
{
	if (!isPKI()) return false;
	const CKeyPKI& pki = static_cast<const CKeyPKI&>(*this);
	QByteArray this_key = pki.rcpt_key;
	QSslKey k = cert.publicKey();
	QByteArray other_key = Crypto::toPublicKeyDer(k);
	qDebug() << "This key:" << this_key.toHex();
	qDebug() << "Other key:" << other_key.toHex();
	if (this_key.isEmpty() || other_key.isEmpty()) return false;
	return this_key == other_key;
}

CKeyCert::CKeyCert(Type _type, const QSslCertificate &c)
	: CKeyPKI(_type)
{
	setCert(c);
	label = [](const SslCertificate &c) {
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

void CKeyCert::setCert(const QSslCertificate &c)
{
	cert = c;
	QSslKey k = c.publicKey();
	rcpt_key = Crypto::toPublicKeyDer(k);
	qDebug() << "Set cert PK:" << rcpt_key.toHex();
	pk_type = (k.algorithm() == QSsl::Rsa) ? PKType::RSA : PKType::ECC;
}

std::shared_ptr<CKeyServer>
CKeyServer::fromKey(QByteArray _key, PKType _pk_type) {
	return std::shared_ptr<CKeyServer>(new CKeyServer(_key, _pk_type));
}

CryptoDoc::CryptoDoc( QObject *parent )
	: QObject(parent)
	, d(new Private)
{
	const_cast<QLoggingCategory&>(CRYPTO()).setEnabled(QtDebugMsg,
													   QFile::exists(QStringLiteral("%1/%2.log").arg(QDir::tempPath(), Application::applicationName())));
}

CryptoDoc::~CryptoDoc() { clear(); delete d; }

bool
CryptoDoc::supportsSymmetricKeys() const
{
	return d->cdoc->version() >= 2;
}

bool CryptoDoc::addKey(std::shared_ptr<CKey> key )
{
	if(d->isEncryptedWarning())
		return false;
	for (std::shared_ptr<CKey> k: d->cdoc->keys) {
		if (k->isTheSameRecipient(*key)) {
			WarningDialog::show(tr("Key already exists"));
			return false;
		}
	}
	d->cdoc->keys.append(key);
	return true;
}

bool CryptoDoc::canDecrypt(const QSslCertificate &cert)
{
	CKey::DecryptionStatus dec_stat = d->cdoc->canDecrypt(cert);
	return (dec_stat == CKey::CAN_DECRYPT) || (dec_stat == CKey::DecryptionStatus::NEED_KEY);
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

bool CryptoDoc::decrypt(std::shared_ptr<CKey> key, const QByteArray& secret)
{
	if( d->fileName.isEmpty() )
	{
		WarningDialog::show(tr("Container is not open"));
		return false;
	}
	if(!d->isEncrypted)
		return true;

	if (key == nullptr) {
		key = d->cdoc->getDecryptionKey(qApp->signer()->tokenauth().cert());
	}
	if((key == nullptr) || (key->isSymmetric() && secret.isEmpty())) {
		WarningDialog::show(tr("You do not have the key to decrypt this document"));
		return false;
	}

	if(d->cdoc->version() == 2 && (key->type == CKey::Type::SERVER) && !Settings::CDOC2_NOTIFICATION.isSet())
	{
		auto *dlg = new WarningDialog(tr("You must enter your PIN code twice in order to decrypt the CDOC2 container. "
										 "The first PIN entry is required for authentication to the key server referenced in the CDOC2 container. "
										 "Second PIN entry is required to decrypt the CDOC2 container."), Application::mainWindow());
		dlg->setCancelText(WarningDialog::Cancel);
		dlg->addButton(WarningDialog::OK, QMessageBox::Ok);
		dlg->addButton(tr("DON'T SHOW AGAIN"), QMessageBox::Ignore);
		switch (dlg->exec())
		{
		case QMessageBox::Ok: break;
		case QMessageBox::Ignore:
			Settings::CDOC2_NOTIFICATION = true;
			break;
		default: return false;
		}
	}

	d->fmk = d->cdoc->getFMK(*key, secret);
#ifndef NDEBUG
	qDebug() << "FMK (Transport key)" << d->fmk.toHex();
#endif
	if(d->fmk.isEmpty()) {
		WarningDialog::show(tr("Failed to decrypt document. Please check your internet connection and network settings."), d->cdoc->lastError);
		return false;
	}

	d->waitForFinished();
	if(d->isEncrypted)
		WarningDialog::show(tr("Error parsing document"));
	if(!d->cdoc->lastError.isEmpty())
		WarningDialog::show(d->cdoc->lastError);

	return !d->isEncrypted;
}

DocumentModel* CryptoDoc::documentModel() const { return d->documents; }

bool CryptoDoc::encrypt( const QString &filename, const QString& label, const QByteArray& secret, uint32_t kdf_iter)
{
	if(!filename.isEmpty()) d->fileName = filename;
	if(d->fileName.isEmpty()) {
		WarningDialog::show(tr("Container is not open"));
		return false;
	}
	// I think the correct semantics is to fail if container is already encrypted
	if(d->isEncrypted) return false;
	if (secret.isEmpty()) {
		// Encrypt for address list
		if(d->cdoc->keys.isEmpty())
		{
			WarningDialog::show(tr("No keys specified"));
			return false;
		}
	} else {
		// Encrypt with symmetric key
		d->label = label;
		d->secret = secret;
		d->kdf_iter = kdf_iter;
	}
	d->waitForFinished();
	d->label.clear();
	d->secret.clear();
	if(d->isEncrypted)
		open(d->fileName);
	else
		WarningDialog::show(tr("Failed to encrypt document. Please check your internet connection and network settings."), d->cdoc->lastError);
	return d->isEncrypted;
}

QString CryptoDoc::fileName() const { return d->fileName; }

QList<std::shared_ptr<CKey>> CryptoDoc::keys() const
{
	return d->cdoc->keys;
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
	if (CDoc2::isCDoc2File(d->fileName)) {
		d->cdoc = CDoc2::load(d->fileName);
	} else if (CDoc1::isCDoc1File(d->fileName)) {
		d->cdoc = CDoc1::load(d->fileName);
	} else {
		WarningDialog::show(tr("Failed to open document"), tr("Unsupported file format"));
		return false;
	}
	d->isEncrypted = bool(d->cdoc);
	// fixme: This seems wrong
	if(!d->isEncrypted)
	{
		WarningDialog::show(tr("Failed to open document"), d->cdoc->lastError);
		return false;
	}
	Application::addRecent( file );
	return true;
}

void CryptoDoc::removeKey( int id )
{
	if(!d->isEncryptedWarning())
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
