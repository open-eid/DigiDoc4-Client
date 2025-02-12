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
#include "Crypto.h"
#include "CDocSupport.h"
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

#include <cdoc/CDocReader.h>
#include <cdoc/CDocWriter.h>
#include <cdoc/Certificate.h>
#include <cdoc/Crypto.h>

using namespace ria::qdigidoc4;

auto toHex = [](const std::vector<uint8_t>& data) -> QString {
	QByteArray ba(reinterpret_cast<const char *>(data.data()), data.size());
	return ba.toHex();
};

std::string
CryptoDoc::labelFromCertificate(const std::vector<uint8_t>& cert)
{
	QSslCertificate kcert(QByteArray(reinterpret_cast<const char *>(cert.data()), cert.size()), QSsl::Der);
	return [](const SslCertificate &c) {
		QString cn = c.subjectInfo(QSslCertificate::CommonName);
		QString gn = c.subjectInfo("GN");
		QString sn = c.subjectInfo("SN");
		if(!gn.isEmpty() || !sn.isEmpty())
			cn = QStringLiteral("%1 %2 %3").arg(gn, sn, c.personalCode());

		int certType = c.type();
		if(certType & SslCertificate::EResidentSubType)
			return QStringLiteral("%1 %2").arg(cn, CryptoDoc::tr("Digi-ID E-RESIDENT")).toStdString();
		if(certType & SslCertificate::DigiIDType)
			return QStringLiteral("%1 %2").arg(cn, CryptoDoc::tr("Digi-ID")).toStdString();
		if(certType & SslCertificate::EstEidType)
			return QStringLiteral("%1 %2").arg(cn, CryptoDoc::tr("ID-CARD")).toStdString();
		return cn.toStdString();
	}(kcert);
}

class CryptoDoc::Private final: public QThread
{
	Q_OBJECT
public:
	bool warnIfNotWritable() const;
	void run() final;
	inline void waitForFinished()
	{
		QEventLoop e;
		connect(this, &Private::finished, &e, &QEventLoop::quit);
		start();
		e.exec();
	}

	std::unique_ptr<libcdoc::CDocReader> reader;
	std::string writer_last_error;

	QString			fileName;
	//bool			encrypted = false;
	//bool isEncrypted() const { return encrypted; }
	bool isEncrypted() const { return reader != nullptr; }
	CDocumentModel	*documents = new CDocumentModel(this);
	QStringList		tempFiles;
	// Decryption data
	QByteArray fmk;
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
		libcdoc::CDocReader *r = libcdoc::CDocReader::createReader(filename, &conf, &crypto, &network);
		if (!r) {
			WarningDialog::show(tr("Failed to open document"), tr("Unsupported file format"));
			return nullptr;
		}
		keys.clear();
        for (auto& key : r->getLocks()) {
            keys.push_back({key, QSslCertificate()});
		}
		return std::unique_ptr<libcdoc::CDocReader>(r);
	}
private:
    bool encrypt();
};

bool CryptoDoc::Private::warnIfNotWritable() const
{
	if(fileName.isEmpty()) {
		WarningDialog::show(CryptoDoc::tr("Container is not open"));
	} else if(reader) {
		WarningDialog::show(CryptoDoc::tr("Container is encrypted"));
	} else {
		return false;
	}
	return true;
}

void CryptoDoc::Private::run()
{
	if(reader) {
		qCDebug(CRYPTO) << "Decrypt" << fileName;
		std::vector<uint8_t> pfmk(fmk.cbegin(), fmk.cend());

		TempListConsumer cons;
		if (reader->decrypt(pfmk, &cons) == libcdoc::OK) {
			files = std::move(cons.files);
			// Success, immediately create writer from reader
			keys.clear();
			writer_last_error.clear();
			reader.reset();
		}
	} else {
        if (encrypt()) {
			// Encryption successful, open new reader
			reader = createCDocReader(fileName.toStdString());
			if (!reader) return;
		}
	}
}

bool
CryptoDoc::Private::encrypt()
{
    qCDebug(CRYPTO) << "Encrypt" << fileName;

    libcdoc::OStreamConsumer ofs(fileName.toStdString());
    if (ofs.isError()) return false;

    StreamListSource slsrc(files);
    std::vector<libcdoc::Recipient> enc_keys;

    std::string keyserver_id;
    if (Settings::CDOC2_DEFAULT && Settings::CDOC2_USE_KEYSERVER) {
        keyserver_id = Settings::CDOC2_DEFAULT_KEYSERVER;
    }
    for (auto& key : keys) {
        QByteArray ba = key.rcpt_cert.toDer();
        std::vector<uint8_t> cert_der = std::vector<uint8_t>(ba.cbegin(), ba.cend());
        QSslKey qkey = key.rcpt_cert.publicKey();
        ba = Crypto::toPublicKeyDer(qkey);
        std::vector<uint8_t> key_der(ba.cbegin(), ba.cend());
        libcdoc::Recipient::PKType pk_type = (qkey.algorithm() == QSsl::KeyAlgorithm::Rsa) ? libcdoc::Recipient::PKType::RSA : libcdoc::Recipient::PKType::ECC;
        if (!keyserver_id.empty()) {
            std::string label = CryptoDoc::labelFromCertificate(cert_der);
            enc_keys.push_back(libcdoc::Recipient::makeServer(label, key_der, pk_type, keyserver_id));
        } else {
            std::string label = CryptoDoc::labelFromCertificate(cert_der);
            enc_keys.push_back(libcdoc::Recipient::makeCertificate(label, cert_der));
        }
    }
    if (!crypto.secret.empty()) {
        auto key = libcdoc::Recipient::makeSymmetric(label.toStdString(), kdf_iter);
        enc_keys.push_back(key);
    }

    libcdoc::CDocWriter *writer = libcdoc::CDocWriter::createWriter(Settings::CDOC2_DEFAULT ? 2 : 1, &ofs, false, &conf, &crypto, &network);
    int result = writer->encrypt(slsrc, enc_keys);
    if (result != libcdoc::OK) {
        writer_last_error = writer->getLastErrorStr();
        std::filesystem::remove(std::filesystem::path(fileName.toStdString()));
    }
    delete writer;
    ofs.close();
    return (result == libcdoc::OK);
}

CDocumentModel::CDocumentModel(CryptoDoc::Private *doc)
: d( doc )
{}

bool CDocumentModel::addFile(const QString &file, const QString &mime)
{
	if(d->warnIfNotWritable()) return false;

	QFileInfo info(file);
	if(info.size() == 0) {
		WarningDialog::show(DocumentModel::tr("Cannot add empty file to the container."));
		return false;
	}
	if(!Settings::CDOC2_DEFAULT && info.size() > 120*1024*1024) {
		WarningDialog::show(tr("Added file(s) exceeds the maximum size limit of the container (∼120MB). "
			"<a href='https://www.id.ee/en/article/encrypting-large-120-mb-files/'>Read more about it</a>"));
		return false;
	}
	std::string fileName(info.fileName().toStdString());
	if(std::any_of(d->files.cbegin(), d->files.cend(),
			[&fileName](const auto &containerFile) { return containerFile.name == fileName; }))
	{
		WarningDialog::show(DocumentModel::tr("Cannot add the file to the envelope. File '%1' is already in container.")
							.arg(FileDialog::normalized(info.fileName())));
		return false;
	}

	auto data = std::make_unique<QFile>(file);
	data->open(QFile::ReadOnly);
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
	WarningDialog::show(tr("Failed to save file '%1'").arg(dst));
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
	if(d->warnIfNotWritable())
		return false;

	if(row >= d->files.size()) {
		WarningDialog::show(DocumentModel::tr("Internal error"));
		return false;
	}

	d->files.erase(d->files.begin() + row);
	emit removed(row);
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

	int zone = FileDialog::fileZone(d->fileName);
	QString fileName = copy(row, path);
	QFileInfo f(fileName);
	if(!f.exists())
		return {};
	FileDialog::setFileZone(fileName, zone);
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

bool CryptoDoc::addEncryptionKey(const QSslCertificate& cert )
{
    if(d->warnIfNotWritable()) {
		return false;
    }
	for (auto& k : d->keys) {
        if (k.rcpt_cert == cert) {
            WarningDialog::show(tr("Recipient with the same key already exists"));
			return false;
		}
	}
    d->keys.push_back({{}, cert});
	return true;
}

bool CryptoDoc::canDecrypt(const QSslCertificate &cert)
{
	if (!d->reader) return false;
	QByteArray der = cert.toDer();
	libcdoc::Lock lock;
    return d->reader->getLockForCert(std::vector<uint8_t>(der.cbegin(), der.cend())) >= 0;
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
}

ContainerState CryptoDoc::state() const
{
	return d->isEncrypted() ? EncryptedContainer : UnencryptedContainer;
}

bool
CryptoDoc::decrypt(const libcdoc::Lock *lock, const QByteArray& secret)
{
	if(d->fileName.isEmpty()) {
		WarningDialog::show(tr("Container is not open"));
		return false;
	}
    if(!d->reader) return true;

    int lock_idx = -1;
    const std::vector<libcdoc::Lock> locks = d->reader->getLocks();
	if (lock == nullptr) {
		QByteArray der = qApp->signer()->tokenauth().cert().toDer();
        lock_idx = d->reader->getLockForCert(std::vector<uint8_t>(der.cbegin(), der.cend()));
        if (lock_idx < 0) {
			WarningDialog::show(tr("You do not have the key to decrypt this document"));
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
        if (lock_idx >= locks.size()) lock_idx = -1;
    }
    if(!lock || (lock->isSymmetric() && secret.isEmpty())) {
		WarningDialog::show(tr("You do not have the key to decrypt this document"));
		return false;
	}

	if(d->reader->version == 2 && (lock->type == libcdoc::Lock::Type::SERVER) && !Settings::CDOC2_NOTIFICATION.isSet())
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

	d->crypto.secret.assign(secret.cbegin(), secret.cend());
	std::vector<uint8_t> fmk;
    if (d->reader->getFMK(fmk, lock_idx) != libcdoc::OK) return false;
	d->fmk = QByteArray(reinterpret_cast<const char *>(fmk.data()), fmk.size());
#ifndef NDEBUG
	qDebug() << "FMK (Transport key)" << d->fmk.toHex();
#endif
	if(d->fmk.isEmpty()) {
		const std::string& msg = d->reader->getLastErrorStr();
		WarningDialog::show(tr("Failed to decrypt document. Please check your internet connection and network settings."), QString::fromStdString(msg));
		return false;
	}

	d->waitForFinished();
	if(d->reader) {
		const std::string& msg = d->reader->getLastErrorStr();
		if (msg.empty()) {
			WarningDialog::show(tr("Error parsing document"));
		} else {
			WarningDialog::show(QString::fromStdString(msg));
		}
	}
	return !d->isEncrypted();
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
	if(d->reader) return false;
	if (secret.isEmpty()) {
		// Encrypt for address list
		if(d->keys.empty())
		{
			WarningDialog::show(tr("No keys specified"));
			return false;
		}
	} else {
		// Encrypt with symmetric key
		d->label = label;
		d->crypto.secret.assign(secret.cbegin(), secret.cend());
		d->kdf_iter = kdf_iter;
	}
	d->waitForFinished();
	d->label.clear();
	d->crypto.secret.clear();
	if(d->isEncrypted()) {
		open(d->fileName);
	} else {
		WarningDialog::show(tr("Failed to encrypt document. Please check your internet connection and network settings."), QString::fromStdString(d->writer_last_error));
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
	if(!d->isEncrypted())
	{
		d->fileName = to;
		return true;
	}

	return false;
}

bool CryptoDoc::open( const QString &file )
{
	clear(file);
	d->reader = d->createCDocReader(file.toStdString());
	if (!d->reader) return false;
	d->writer_last_error.clear();
	// fixme: This seems wrong
	//if(!d->isEncrypted()) {
	//	WarningDialog::show(tr("Failed to open document"),
	//						QString::fromStdString(d->cdoc->lastError));
	//	return false;
	//}
	Application::addRecent( file );
	return true;
}

void CryptoDoc::removeKey( int id )
{
	if(!d->warnIfNotWritable())
		d->keys.erase(d->keys.begin() + id);
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
