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
#include "CDocSupport.h"
#include "TokenData.h"
#include "QCryptoBackend.h"
#include "QSigner.h"
#include "Settings.h"
#include "Utils.h"
#include "dialogs/FileDialog.h"
#include "dialogs/WarningDialog.h"

#include <QtCore/QDir>
#include <QtCore/QFileInfo>
#include <QtCore/QRegularExpression>
#include <QtCore/QThread>
#include <QtCore/QUrl>
#include <QtCore/QUrlQuery>
#include <QtGui/QDesktopServices>
#include <QtNetwork/QSslKey>
#include <QtWidgets/QMessageBox>

#include <cdoc/CDocReader.h>
#include <cdoc/CDocWriter.h>
#include <cdoc/Certificate.h>
#include <cdoc/Crypto.h>

using namespace ria::qdigidoc4;

Q_LOGGING_CATEGORY(CRYPTO, "CRYPTO")

struct CryptoDoc::Private
{
	bool isEncryptedWarning(const QString &title) const;

	std::unique_ptr<libcdoc::CDocReader> reader;

	QString			fileName;
	bool isEncrypted() const { return reader != nullptr; }
	CDocumentModel	*documents = new CDocumentModel(this);
	QStringList		tempFiles;

	// libcdoc handlers
	DDConfiguration conf;
	DDCryptoBackend crypto;
	DDNetworkBackend network;

	std::vector<IOEntry> files;
	std::vector<CDKey> keys;

	void createCDocReader(const QString &filename) {
		reader = std::unique_ptr<libcdoc::CDocReader>(libcdoc::CDocReader::createReader(filename.toStdString(), &conf, &crypto, &network));
		if (!reader) 
			return;
		keys.clear();
		for (auto& key : reader->getLocks()) {
			keys.push_back({key, QSslCertificate()});
		}
	}
};

bool CryptoDoc::Private::isEncryptedWarning(const QString &title) const
{
	if(fileName.isEmpty() )
		WarningDialog::create()->withTitle(title)->withText(CryptoDoc::tr("Container is not open"))->open();
	if(isEncrypted())
		WarningDialog::create()->withTitle(title)->withText(CryptoDoc::tr("Container is encrypted"))->open();
	return fileName.isEmpty() || isEncrypted();
}

CDocumentModel::CDocumentModel(CryptoDoc::Private *doc) : d(doc) {}

bool CDocumentModel::addFile(const QString &file, const QString &mime)
{
	if(d->isEncryptedWarning(DocumentModel::tr("Failed to add file")))
		return false;

	QFileInfo info(file);
	if(!addFileCheck(d->fileName, info))
		return false;

	if(d->fileName.last(3) == QLatin1String("cdoc") && info.size() > 120*1024*1024)
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
	d->files.push_back({
		QFileInfo(file).fileName().toStdString(),
		mime.toStdString(),
		data->size(),
		std::move(data),
	});
	emit added(FileDialog::normalized(info.fileName()));
	return true;
}

void CDocumentModel::addTempReference(const QString &file)
{
	d->tempFiles.append(file);
}

QString CDocumentModel::copy(int row, const QString &dst) const
{
	auto &file = d->files.at(row);
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
	return FileDialog::normalized(QString::fromStdString(d->files.at(row).name));
}

quint64 CDocumentModel::fileSize(int row) const
{
	return d->files.at(row).size;
}

QString CDocumentModel::mime(int row) const
{
	return FileDialog::normalized(QString::fromStdString(d->files.at(row).mime));
}

void CDocumentModel::open(int row)
{
	if(d->isEncrypted())
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

	if(d->files.empty() || row >= d->files.size())
	{
		WarningDialog::create()
			->withTitle(DocumentModel::tr("Failed remove document from container"))
			->withText(DocumentModel::tr("Internal error"))
			->open();
		return false;
	}

	d->files.erase(d->files.cbegin() + row);
	return true;
}

int CDocumentModel::rowCount() const
{
	return int(d->files.size());
}

QString CDocumentModel::save(int row, const QString &path) const
{
	if(d->isEncrypted())
		return {};

	QString fileName = copy(row, path);
	QFileInfo f(fileName);
	if(!f.exists())
		return {};
	FileDialog::setFileZone(fileName, d->fileName);
	return fileName;
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
	return !d->reader && Settings::CDOC2_DEFAULT;
}

bool CryptoDoc::addEncryptionKey(const QSslCertificate &cert) {
	if(d->isEncryptedWarning(tr("Failed to add key")))
		return false;
	for (auto &k : d->keys) {
		if (k.rcpt_cert == cert) {
			WarningDialog::create()
				->withTitle(tr("Failed to add key"))
				->withText(tr("Key already exists"))
				->open();
			return false;
		}
	}
	d->keys.push_back({{}, cert});
	return true;
}

bool CryptoDoc::canDecrypt(const QSslCertificate &cert) {
	if (!d->reader)
		return false;
	QByteArray der = cert.toDer();
	return d->reader->getLockForCert(
		std::vector<uint8_t>(der.cbegin(), der.cend())) >= 0;
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
	d->reader.reset();
	d->fileName = file;
	d->files.clear();
}

ContainerState CryptoDoc::state() const
{
	return d->isEncrypted() ? EncryptedContainer : UnencryptedContainer;
}

bool CryptoDoc::decrypt(const libcdoc::Lock *lock, const QByteArray& secret)
{
	if(!d->reader)
	{
		WarningDialog::create()
			->withTitle(QSigner::tr("Failed to decrypt document"))
			->withText(tr("Container is not open"))
			->open();
		return false;
	}

	int lock_idx = -1;
	const std::vector<libcdoc::Lock> &locks = d->reader->getLocks();
	if (lock == nullptr) {
		QByteArray der = qApp->signer()->tokenauth().cert().toDer();
		lock_idx = d->reader->getLockForCert(
			std::vector<uint8_t>(der.cbegin(), der.cend()));
		if (lock_idx < 0) {
			WarningDialog::create()
				->withTitle(QSigner::tr("Failed to decrypt document"))
				->withText(tr("You do not have the key to decrypt this document"))
				->open();
			return false;
		}
		lock = &locks.at(lock_idx);
	} else {
		for (lock_idx = 0; lock_idx < locks.size(); lock_idx++) {
			if (lock->label == locks[lock_idx].label) {
				lock = &locks.at(lock_idx);
				break;
			}
		}
		if (lock_idx >= locks.size())
			lock_idx = -1;
	}
	if (!lock || (lock->isSymmetric() && secret.isEmpty())) {
		WarningDialog::create()
			->withTitle(QSigner::tr("Failed to decrypt document"))
			->withText(tr("You do not have the key to decrypt this document"))
			->open();
		return false;
	}

	if (d->reader->version == 2 &&
		(lock->type == libcdoc::Lock::Type::SERVER) &&
		!Settings::CDOC2_NOTIFICATION.isSet()) {
		auto *dlg = WarningDialog::create()
			->withTitle(tr("You must enter your PIN code twice in order to decrypt the CDOC2 container"))
			->withText(tr(
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
		default:
			return false;
		}
	}

	d->crypto.secret.assign(secret.cbegin(), secret.cend());

	TempListConsumer cons;
	libcdoc::result_t result = waitFor([&]{
		std::vector<uint8_t> fmk;
		auto scope = qScopeGuard([&] {
			std::fill(fmk.begin(), fmk.end(), 0);
		});
		libcdoc::result_t result = d->reader->getFMK(fmk, lock_idx);
		qCDebug(CRYPTO) << "getFMK result: " << result << " " << QString::fromStdString(d->reader->getLastErrorStr());
		if (result != libcdoc::OK)
			return result;
		result = d->reader->decrypt(fmk, &cons);
		qCDebug(CRYPTO) << "Decryption result: " << result << " " << QString::fromStdString(d->reader->getLastErrorStr());
		return result;
	});
	if (result != libcdoc::OK) {
		const std::string &msg = d->reader->getLastErrorStr();
		WarningDialog::create()
			->withTitle(QSigner::tr("Failed to decrypt document"))
			->withText(tr("Please check your internet connection and network settings."))
			->withDetails(QString::fromStdString(msg))
			->open();
		return false;
	}

	d->files = std::move(cons.files);
	// Success, immediately create writer from reader
	d->keys.clear();
	d->reader.reset();
	return !d->isEncrypted();
}

DocumentModel *CryptoDoc::documentModel() const { return d->documents; }

bool CryptoDoc::encrypt( const QString &filename, const QString& label, const QByteArray& secret)
{
	// I think the correct semantics is to fail if container is already encrypted
	if(d->reader)
		return false;
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
	if(secret.isEmpty() && d->keys.empty())
	{
		WarningDialog::create()
			->withTitle(tr("Failed to encrypt document"))
			->withText(tr("No keys specified"))
			->open();
		return false;
	}
	QString writer_last_error;
	libcdoc::result_t result = waitFor([&] -> libcdoc::result_t {
		qCDebug(CRYPTO) << "Encrypt" << d->fileName;
		auto writer = std::unique_ptr<libcdoc::CDocWriter>(libcdoc::CDocWriter::createWriter(
			Settings::CDOC2_DEFAULT ? 2 : 1, d->fileName.toStdString(), &d->conf, &d->crypto, &d->network));
		if (!writer)
			return libcdoc::OUTPUT_ERROR;
		std::vector<libcdoc::Recipient> enc_keys;
		std::string keyserver_id;
		if (Settings::CDOC2_DEFAULT && Settings::CDOC2_USE_KEYSERVER) {
			keyserver_id = Settings::CDOC2_DEFAULT_KEYSERVER;
		}
		// Encrypt for address list
		for (const auto &key : d->keys) {
			QByteArray ba = key.rcpt_cert.toDer();
			enc_keys.push_back(keyserver_id.empty() ?
				libcdoc::Recipient::makeCertificate({}, {ba.cbegin(), ba.cend()}) :
				libcdoc::Recipient::makeServer({}, {ba.cbegin(), ba.cend()}, keyserver_id));
		}
		// Encrypt with symmetric key
		if (!secret.isEmpty()) {
			// NIST recommends at least 600000 iterations for PBKDF2 with SHA-256, see https://csrc.nist.gov/publications/detail/sp/800-132/final
			d->crypto.secret.assign(secret.cbegin(), secret.cend());
			enc_keys.push_back(libcdoc::Recipient::makeSymmetric(label.toStdString(), 600000));
		}
		StreamListSource slsrc(d->files);
		libcdoc::result_t result = writer->encrypt(slsrc, enc_keys);
		writer_last_error = QString::fromStdString(writer->getLastErrorStr());
		qCDebug(CRYPTO) << "Encryption result: " << result << ' ' << writer_last_error;
		if (result == libcdoc::OK) // Encryption successful, open new reader
			d->createCDocReader(d->fileName);
		else if(QFile::exists(d->fileName))
			QFile::remove(d->fileName);
		return result;
	});
	d->crypto.secret.clear();
	if (result != libcdoc::OK) {
		WarningDialog::create()
			->withTitle(tr("Failed to encrypt document"))
			->withText(tr("Please check your internet connection and network settings."))
			->withDetails(writer_last_error)
			->open();
	} else if(!d->reader) {
		WarningDialog::create()
			->withTitle(CryptoDoc::tr("Failed to open document"))
			->withText(CryptoDoc::tr("Unsupported file format"))
			->open();
		return false;
	}

	return d->isEncrypted();
}

QString CryptoDoc::fileName() const { return d->fileName; }

const std::vector<CDKey>&
CryptoDoc::keys() const
{
	return d->keys;
}

bool CryptoDoc::move(const QString &to)
{
	if(d->isEncrypted())
		return false;
	d->fileName = to;
	return true;
}

bool CryptoDoc::open(const QString &file)
{
	clear(file);
	d->createCDocReader(file);
	if(!d->reader) {
		WarningDialog::create()
			->withTitle(CryptoDoc::tr("Failed to open document"))
			->withText(CryptoDoc::tr("Unsupported file format"))
			->open();
		return false;
	}
	std::vector<libcdoc::FileInfo> files = CDocSupport::getCDocFileList(file);
	for (auto& f : files) {
		d->files.push_back({f.name, {}, f.size, {}});
	}
	Application::addRecent( file );
	return true;
}

void CryptoDoc::removeKey(unsigned int id)
{
	if(!d->isEncryptedWarning(tr("Failed to remove key")))
		d->keys.erase(d->keys.begin() + id);
}

void CryptoDoc::clearKeys()
{
	if(!d->isEncryptedWarning(tr("Failed to remove key")))
		d->keys.clear();
}

bool CryptoDoc::saveCopy(const QString &filename)
{
	if(QFileInfo(filename) == QFileInfo(d->fileName))
		return true;
	if(QFile::exists(filename))
		QFile::remove(filename);
	return QFile::copy(d->fileName, filename);
}
