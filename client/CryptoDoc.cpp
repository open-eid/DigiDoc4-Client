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

auto toHex = [](const std::vector<uint8_t>& data) -> QString {
	return QByteArray::fromRawData(reinterpret_cast<const char *>(data.data()), data.size()).toHex();
};

struct CryptoDoc::Private
{
	bool isEncryptedWarning(const QString &title) const;
	inline libcdoc::result_t decrypt(unsigned int lock_idx) {
		TempListConsumer cons;
		libcdoc::result_t result = waitFor([&]{
			std::vector<uint8_t> fmk;
			libcdoc::result_t result = reader->getFMK(fmk, lock_idx);
			qCDebug(CRYPTO) << "getFMK result: " << result << " " << QString::fromStdString(reader->getLastErrorStr());
			if (result != libcdoc::OK) return result;
			result = reader->decrypt(fmk, &cons);
			std::fill(fmk.begin(), fmk.end(), 0);
			qCDebug(CRYPTO) << "Decryption result: " << result << " " << QString::fromStdString(reader->getLastErrorStr());
			return result;
		});
		if (result == libcdoc::OK) {
			files = std::move(cons.files);
			// Success, immediately create writer from reader
			keys.clear();
			writer_last_error.clear();
			reader.reset();
		}
		return result;
	}

	inline libcdoc::result_t encrypt() {
		libcdoc::result_t res = waitFor([&]{
			qCDebug(CRYPTO) << "CryptoDoc::Private::encrypt, thread id: " << QThread::currentThreadId();
			qCDebug(CRYPTO) << "Encrypt" << fileName;
			libcdoc::OStreamConsumer ofs(fileName.toStdString());
			if (ofs.isError())
				return (libcdoc::result_t) libcdoc::OUTPUT_ERROR;
			StreamListSource slsrc(files);
			std::vector<libcdoc::Recipient> enc_keys;
			std::string keyserver_id;
			if (Settings::CDOC2_DEFAULT && Settings::CDOC2_USE_KEYSERVER) {
				keyserver_id = Settings::CDOC2_DEFAULT_KEYSERVER;
			}
			for (auto &key : keys) {
				QByteArray ba = key.rcpt_cert.toDer();
				if (keyserver_id.empty()) {
					enc_keys.push_back(
						libcdoc::Recipient::makeCertificate({}, {ba.cbegin(), ba.cend()}));
				} else {
					enc_keys.push_back(
						libcdoc::Recipient::makeServer({}, {ba.cbegin(), ba.cend()}, keyserver_id));
				}
			}
			if (!crypto.secret.empty()) {
				auto key =
					libcdoc::Recipient::makeSymmetric(label.toStdString(), kdf_iter);
				enc_keys.push_back(key);
			}
			libcdoc::CDocWriter *writer = libcdoc::CDocWriter::createWriter(
				Settings::CDOC2_DEFAULT ? 2 : 1, &ofs, false, &conf, &crypto, &network);
			libcdoc::result_t result = writer->encrypt(slsrc, enc_keys);
			if (result != libcdoc::OK) {
				writer_last_error = writer->getLastErrorStr();
				std::filesystem::remove(std::filesystem::path(fileName.toStdString()));
			}
			qCDebug(CRYPTO) << "Encryption result: " << result << " " << QString::fromStdString(writer->getLastErrorStr());
			delete writer;
			ofs.close();
			if (result == libcdoc::OK) {
				// Encryption successful, open new reader
				reader = createCDocReader(fileName.toStdString());
			}
			return result;
		});
		return res;
	}

	std::unique_ptr<libcdoc::CDocReader> reader;
	std::string writer_last_error;

	QString			fileName;
	bool isEncrypted() const { return reader != nullptr; }
	CDocumentModel	*documents = new CDocumentModel(this);
	QStringList		tempFiles;
	// Encryption data
	QString label;
	uint32_t kdf_iter;

	// libcdoc handlers
	DDConfiguration conf;
	DDCryptoBackend crypto;
	DDNetworkBackend network;

	std::vector<IOEntry> files;
	std::vector<CDKey> keys;

	const std::vector<IOEntry> &getFiles() {
		return files;
	}
	std::unique_ptr<libcdoc::CDocReader> createCDocReader(const std::string& filename) {
		std::unique_ptr<libcdoc::CDocReader> r(libcdoc::CDocReader::createReader(filename, &conf, &crypto, &network));
		if (!r) {
			WarningDialog::create()
				->withTitle(CryptoDoc::tr("Failed to open document"))
				->withText(CryptoDoc::tr("Unsupported file format"))
				->open();
			return nullptr;
		}
		keys.clear();
		for (auto& key : r->getLocks()) {
			keys.push_back({key, QSslCertificate()});
		}
		return r;
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
	return FileDialog::normalized(QString::fromStdString(d->getFiles().at(row).name));
}

quint64 CDocumentModel::fileSize(int row) const
{
	return d->getFiles().at(row).size;
}

QString CDocumentModel::mime(int row) const
{
	return FileDialog::normalized(QString::fromStdString(d->getFiles().at(row).mime));
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
	return int(d->getFiles().size());
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
	d->writer_last_error.clear();
	d->files.clear();
}

ContainerState CryptoDoc::state() const
{
	return d->isEncrypted() ? EncryptedContainer : UnencryptedContainer;
}

bool CryptoDoc::decrypt(const libcdoc::Lock *lock, const QByteArray& secret)
{
	if( d->fileName.isEmpty() )
	{
		WarningDialog::create()
			->withTitle(QSigner::tr("Failed to decrypt document"))
			->withText(tr("Container is not open"))
			->open();
		return false;
	}
	if (!d->reader)
		return true;

	int lock_idx = -1;
	const std::vector<libcdoc::Lock> locks = d->reader->getLocks();
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
		default:
			return false;
		}
	}

	d->crypto.secret.assign(secret.cbegin(), secret.cend());

	libcdoc::result_t result = d->decrypt(lock_idx);
	if (result != libcdoc::OK) {
		const std::string &msg = d->reader->getLastErrorStr();
		WarningDialog::create()
			->withTitle(QSigner::tr("Failed to decrypt document"))
			->withText(tr("Please check your internet connection and network settings."))
			->withDetails(QString::fromStdString(msg))
			->open();
		return false;
	}

	return !d->isEncrypted();
}

DocumentModel *CryptoDoc::documentModel() const { return d->documents; }

bool CryptoDoc::encrypt( const QString &filename, const QString& label, const QByteArray& secret, uint32_t kdf_iter)
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
	// I think the correct semantics is to fail if container is already encrypted
	if(d->reader) return false;
	if (!secret.isEmpty()) {
		// Encrypt with symmetric key
		d->label = label;
		d->crypto.secret.assign(secret.cbegin(), secret.cend());
		d->kdf_iter = kdf_iter;
	}
	// Encrypt for address list
	else if(d->keys.empty())
	{
		WarningDialog::create()
			->withTitle(tr("Failed to encrypt document"))
			->withText(tr("No keys specified"))
			->open();
		return false;
	}
	libcdoc::result_t result = d->encrypt();
	d->label.clear();
	d->crypto.secret.clear();
	if (result != libcdoc::OK) {
		WarningDialog::create()
			->withTitle(tr("Failed to encrypt document"))
			->withText(tr("Please check your internet connection and network settings."))
			->withDetails(QString::fromStdString(d->writer_last_error))
			->open();
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
	d->writer_last_error.clear();
	clear(file);
	d->reader = d->createCDocReader(file.toStdString());
	if (!d->reader)
		return false;
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
