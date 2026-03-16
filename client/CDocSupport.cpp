/*
 * QDigiDoc4
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

#include <QtCore/QBuffer>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <QtCore/QtEndian>
#include <QtCore/QTemporaryFile>
#include <QtNetwork/QNetworkAccessManager>
#include <QtNetwork/QSslKey>
#include <QLoggingCategory>
#include <QXmlStreamReader>

#include "Application.h"
#include "CheckConnection.h"
#include "QCryptoBackend.h"
#include "QSigner.h"
#include "Settings.h"
#include "TokenData.h"
#include "Utils.h"
#include "effects/FadeInNotification.h"
#include <cdoc/CDocReader.h>
#include <cdoc/Lock.h>
#include <cdoc/Recipient.h>

#include "CDocSupport.h"

#if QT_VERSION < QT_VERSION_CHECK(6, 5, 0)
QDebug operator<<(QDebug d, std::string_view str) {
	return d << QUtf8StringView(str);
}
#endif

static QByteArray toByteArray(const std::vector<uint8_t> &data) {
	return QByteArray::fromRawData(reinterpret_cast<const char *>(data.data()), data.size());
}

std::vector<libcdoc::FileInfo>
CDocSupport::getCDocFileList(const QString &filename)
{
	std::vector<libcdoc::FileInfo> files;
	if (libcdoc::CDocReader::getCDocFileVersion(filename.toStdString()) != 1)
		return files;
	QFile ifs(filename);
	if(!ifs.open(QIODevice::ReadOnly))
		return files;
	QXmlStreamReader xml(&ifs);
	while (xml.readNextStartElement()) {
		if (xml.name() == QLatin1String("EncryptedData")) {
			while (xml.readNextStartElement()) {
				if (xml.name() == QLatin1String("EncryptionProperties")) {
					while (xml.readNextStartElement()) {
						if (xml.name() == QLatin1String("EncryptionProperty")) {
							if (xml.attributes().value(QStringLiteral("Name")) == QLatin1String("orig_file")) {
								QString content = xml.readElementText();
								auto list = content.split('|');
								files.push_back({list.at(0).toStdString(), list.at(1).toInt()});
							} else {
								xml.skipCurrentElement();
							}
						} else {
							xml.skipCurrentElement();
						}
					}
				} else {
					xml.skipCurrentElement();
				}
			}
		} else {
			xml.skipCurrentElement();
		}
	}

	return files;
}

static libcdoc::result_t
getDecryptStatus(const std::vector<uint8_t>& result, QCryptoBackend::PinStatus pin_status)
{
	switch (pin_status) {
	case QCryptoBackend::PinOK:
		return (result.empty()) ? DDCryptoBackend::BACKEND_ERROR : libcdoc::OK;
	case QCryptoBackend::PinCanceled:
		return DDCryptoBackend::PIN_CANCELED;
	case QCryptoBackend::PinIncorrect:
		return DDCryptoBackend::PIN_INCORRECT;
	case QCryptoBackend::PinLocked:
		return DDCryptoBackend::PIN_LOCKED;
	default:
		return DDCryptoBackend::BACKEND_ERROR;
	}
}

libcdoc::result_t
DDCryptoBackend::decryptRSA(std::vector<uint8_t>& result, const std::vector<uint8_t> &data, bool oaep, unsigned int idx)
{
	QCryptoBackend::PinStatus pin_status;
	QByteArray qkek = qApp->signer()->decrypt([qdata = toByteArray(data), &oaep](QCryptoBackend *backend) {
		return backend->decrypt(qdata, oaep);
	}, pin_status);
	result.assign(qkek.cbegin(), qkek.cend());
	return getDecryptStatus(result, pin_status);
}

libcdoc::result_t
DDCryptoBackend::deriveConcatKDF(std::vector<uint8_t>& dst, const std::vector<uint8_t> &publicKey, const std::string &digest,
								 const std::vector<uint8_t> &algorithmID, const std::vector<uint8_t> &partyUInfo, const std::vector<uint8_t> &partyVInfo, unsigned int idx)
{
	QCryptoBackend::PinStatus pin_status;
	QByteArray decryptedKey = qApp->signer()->decrypt([&publicKey, &digest, &algorithmID, &partyUInfo, &partyVInfo](QCryptoBackend *backend) {
		static const QHash<std::string_view, QCryptographicHash::Algorithm> SHA_MTH{
			{"http://www.w3.org/2001/04/xmlenc#sha256", QCryptographicHash::Sha256},
			{"http://www.w3.org/2001/04/xmlenc#sha384", QCryptographicHash::Sha384},
			{"http://www.w3.org/2001/04/xmlenc#sha512", QCryptographicHash::Sha512}
		};
		return backend->deriveConcatKDF(toByteArray(publicKey), SHA_MTH.value(digest),
			toByteArray(algorithmID), toByteArray(partyUInfo), toByteArray(partyVInfo));
	}, pin_status);
	dst.assign(decryptedKey.cbegin(), decryptedKey.cend());
	return getDecryptStatus(dst, pin_status);
}

libcdoc::result_t
DDCryptoBackend::deriveHMACExtract(std::vector<uint8_t>& dst, const std::vector<uint8_t> &key_material, const std::vector<uint8_t> &salt, unsigned int idx)
{
	QCryptoBackend::PinStatus pin_status;
	QByteArray qkekpm = qApp->signer()->decrypt([qkey_material = toByteArray(key_material), qsalt = toByteArray(salt)](QCryptoBackend *backend) {
		return backend->deriveHMACExtract(qkey_material, qsalt, ECC_KEY_LEN);
	}, pin_status);
	dst = std::vector<uint8_t>(qkekpm.cbegin(), qkekpm.cend());
	return getDecryptStatus(dst, pin_status);
}

libcdoc::result_t
DDCryptoBackend::getSecret(std::vector<uint8_t>& _secret, unsigned int idx)
{
	_secret = secret;
	return libcdoc::OK;
}

std::string
DDCryptoBackend::getLastErrorStr(libcdoc::result_t code) const
{
	switch (code) {
		case PIN_CANCELED:
			return "PIN entry canceled";
		case PIN_INCORRECT:
			return "PIN incorrect";
		case PIN_LOCKED:
			return "PIN locked";
		case BACKEND_ERROR:
			return qApp->signer()->getLastErrorStr().toStdString();
	}
	return libcdoc::CryptoBackend::getLastErrorStr(code);
}

static bool
checkConnection()
{
	if(CheckConnection().check()) {
		return true;
	}
	return dispatchToMain([] {
		FadeInNotification::error(Application::mainWindow()->findChild<QWidget*>(QStringLiteral("topBar")),
								  QCoreApplication::translate("MainWindow", "Check internet connection"));
		return false;
	});
}

std::string
DDConfiguration::getValue(std::string_view domain, std::string_view param) const
{
	std::string def = Settings::CDOC2_DEFAULT_KEYSERVER;
	if (param == libcdoc::Configuration::KEYSERVER_SEND_URL) {
#ifdef CONFIG_URL
		QJsonObject list = Application::confValue(QLatin1String("CDOC2-CONF")).toObject();
		QJsonObject data = list.value(QLatin1String(domain.data(), domain.size())).toObject();
		QString url = data.value(QLatin1String("POST")).toString(Settings::CDOC2_POST);
		return url.toStdString();
#else
		QString url = Settings::CDOC2_POST;
		return url.toStdString();
#endif
	} else if (param == libcdoc::Configuration::KEYSERVER_FETCH_URL) {
#ifdef CONFIG_URL
		QJsonObject list = Application::confValue(QLatin1String("CDOC2-CONF")).toObject();
		QJsonObject data = list.value(QLatin1String(domain.data(), domain.size())).toObject();
		QString url = data.value(QLatin1String("FETCH")).toString(Settings::CDOC2_GET);
		return url.toStdString();
#else
		QString url = Settings::CDOC2_GET;
		return url.toStdString();
#endif
	}
	return {};
}

std::string
DDNetworkBackend::getLastErrorStr(libcdoc::result_t code) const
{
	if (code == BACKEND_ERROR) return last_error;
	return libcdoc::NetworkBackend::getLastErrorStr(code);
}

libcdoc::result_t DDNetworkBackend::sendKey(
	libcdoc::NetworkBackend::CapsuleInfo &dst, const std::string &url,
	const std::vector<uint8_t> &rcpt_key,
	const std::vector<uint8_t> &key_material, const std::string &type, uint64_t expiry_ts) {
	QNetworkRequest req(QString::fromStdString(url + "/key-capsules"));
	req.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
	if (expiry_ts) {
		req.setRawHeader("x-expiry-time", QDateTime::fromSecsSinceEpoch(expiry_ts).toString(Qt::ISODate).toLatin1());
	}
	if (!checkConnection()) {
		last_error = "No connection";
		return BACKEND_ERROR;
	}
	QScopedPointer<QNetworkAccessManager, QScopedPointerDeleteLater> nam(CheckConnection::setupNAM(req, Settings::CDOC2_POST_CERT));
	QNetworkReply *reply = nam->post(req, QJsonDocument({
		{QLatin1String("recipient_id"), QLatin1String(toByteArray(rcpt_key).toBase64())},
		{QLatin1String("ephemeral_key_material"), QLatin1String(toByteArray(key_material).toBase64())},
		{QLatin1String("capsule_type"), QLatin1String(type.c_str())},
	}).toJson());
	QEventLoop e;
	connect(reply, &QNetworkReply::finished, &e, &QEventLoop::quit);
	e.exec();
	QNetworkReply::NetworkError n_err = reply->error();
	if (n_err != QNetworkReply::NoError) {
		last_error = reply->errorString().toStdString();
		return BACKEND_ERROR;
	}
	int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
	if (status != 201) {
		last_error = reply->errorString().toStdString();
		return BACKEND_ERROR;
	}
	QString tr_id;
	tr_id = QString::fromLatin1(reply->rawHeader("Location")).remove(QLatin1String("/key-capsules/"));
	if (tr_id.isEmpty()) {
		last_error = "Failed to post key capsule";
		return BACKEND_ERROR;
	}
	dst.transaction_id = tr_id.toStdString();

	if (const QByteArray &expiry = reply->rawHeader("x-expiry-time"); !expiry.isEmpty()) {
		dst.expiry_time = QDateTime::fromString(QLatin1String(expiry), Qt::ISODate).toSecsSinceEpoch();
	} else {
		dst.expiry_time = expiry_ts;
	}
	return libcdoc::OK;
};

libcdoc::result_t
DDNetworkBackend::fetchKey(std::vector<uint8_t> &result,
						   const std::string &url,
						   const std::string &transaction_id) {
	QNetworkRequest req(QStringLiteral("%1/key-capsules/%2").arg(QString::fromStdString(url), QLatin1String(transaction_id.c_str())));
	req.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
	if(!checkConnection()) {
		last_error = "No connection";
		return BACKEND_ERROR;
	}
	auto authKey = dispatchToMain(&QSigner::key, qApp->signer());
	QScopedPointer<QNetworkAccessManager,QScopedPointerDeleteLater> nam(
				CheckConnection::setupNAM(req, qApp->signer()->tokenauth().cert(), authKey, Settings::CDOC2_GET_CERT));
	QEventLoop e;
	QNetworkReply *reply = nam->get(req);
	connect(reply, &QNetworkReply::finished, &e, &QEventLoop::quit);
	e.exec();
	if(authKey.handle()) {
		qApp->signer()->logout();
	}

	if(reply->error() != QNetworkReply::NoError && reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() != 201) {
		last_error = reply->errorString().toStdString();
		return BACKEND_ERROR;
	}
	QJsonObject json = QJsonDocument::fromJson(reply->readAll()).object();
	QByteArray key_material = QByteArray::fromBase64(json.value(QLatin1String("ephemeral_key_material")).toString().toLatin1());
	result.assign(key_material.cbegin(), key_material.cend());
	return libcdoc::OK;
}

DDCDocLogger::~DDCDocLogger()
{
	if (ofs.is_open()) {
		ofs.close();
	}
}

static constexpr std::string_view
level2Str(libcdoc::LogLevel level) noexcept
{
	switch (level) {
		using enum libcdoc::LogLevel;
	case LEVEL_FATAL: return "FATAL";
	case LEVEL_ERROR: return "ERROR";
	case LEVEL_WARNING: return "WARNING";
	case LEVEL_INFO: return "INFO";
	case LEVEL_DEBUG: return "DEBUG";
	case LEVEL_TRACE: return "TRACE";
	default: return "UNKNWON";
	}
}

void DDCDocLogger::logMessage(libcdoc::LogLevel level, std::string_view file, int line, std::string_view message) {
	if (!ofs.is_open()) {
		return;
	}
	auto time = QDateTime::currentDateTime().toString(QStringLiteral("[yyyy-MM-dd hh:mm:ss] "));
	if (auto pos = file.find_last_of("\\/"); pos != std::string_view::npos)
		file = file.substr(pos);
 
	ofs << time.toStdString() << level2Str(level) << ' ' << file << ':' << line << ' ' << message << std::endl;
}

DDCDocLogger *
DDCDocLogger::getLogger()
{
	static DDCDocLogger logger;
	return &logger;
}

void DDCDocLogger::setUpLogger(const std::string& path)
{
	DDCDocLogger *logger = getLogger();
	logger->setMinLogLevel(libcdoc::LEVEL_WARNING);
	logger->ofs.open(path, std::ios_base::app);
	libcdoc::setLogger(logger);
}

void DDCDocLogger::setLogLevel(libcdoc::LogLevel level)
{
	DDCDocLogger *logger = getLogger();
	logger->setMinLogLevel(level);
}

TempListConsumer::~TempListConsumer()
{
	if (!files.empty()) {
		IOEntry& file = files.back();
		file.data->close();
	}
}

libcdoc::result_t TempListConsumer::write(const uint8_t *src, size_t size) noexcept {
	if (files.empty())
		return libcdoc::OUTPUT_ERROR;
	IOEntry &file = files.back();
	if (!file.data->isWritable())
		return libcdoc::OUTPUT_ERROR;
	if (file.data->write((const char *)src, size) != size)
		return libcdoc::OUTPUT_STREAM_ERROR;
	file.size += size;
	return size;
}

libcdoc::result_t TempListConsumer::close() noexcept {
	if (files.empty())
		return libcdoc::OUTPUT_ERROR;
	IOEntry &file = files.back();
	if (!file.data->isWritable())
		return libcdoc::OUTPUT_ERROR;
	return libcdoc::OK;
}

bool
TempListConsumer::isError() noexcept
{
	if (files.empty()) return false;
	const IOEntry& file = files.back();
	return !file.data->isWritable();
}

libcdoc::result_t
TempListConsumer::open(const std::string& name, int64_t size)
{
	std::string truncated = name;
	if (truncated.starts_with("./PaxHeaders.X/"))
		truncated = truncated.substr(15);
	IOEntry io({truncated, "application/octet-stream", 0, {}});
	if ((size < 0) || (size > MAX_VEC_SIZE)) {
		io.data = std::make_unique<QTemporaryFile>();
	} else {
		io.data = std::make_unique<QBuffer>();
	}
	io.data->open(QIODevice::ReadWrite);
	files.push_back(std::move(io));
	return libcdoc::OK;
}

StreamListSource::StreamListSource(const std::vector<IOEntry>& files) : _files(files)
{
}

libcdoc::result_t
StreamListSource::read(uint8_t *dst, size_t size) noexcept
{
	if ((_current < 0) || (_current >= _files.size())) return 0;
	return _files[_current].data->read((char *) dst, size);
}

bool
StreamListSource::isError() noexcept
{
	if ((_current < 0) || (_current >= _files.size())) return 0;
	return _files[_current].data->isReadable();
}

bool
StreamListSource::isEof() noexcept
{
	if (_current < 0) return false;
	if (_current >= _files.size()) return true;
	return _files[_current].data->atEnd();
}

libcdoc::result_t
StreamListSource::getNumComponents()
{
	return _files.size();
}

libcdoc::result_t
StreamListSource::next(std::string& name, int64_t& size)
{
	++_current;
	if (_current >= _files.size()) return libcdoc::END_OF_STREAM;
	_files[_current].data->seek(0);
	name = _files[_current].name;
	size = _files[_current].size;
	return libcdoc::OK;
}

libcdoc::Recipient
makeFromLock(const libcdoc::Lock& lock, const std::string& server_id)
{
	switch (lock.type) {
	case libcdoc::Lock::CDOC1:
		return libcdoc::Recipient::makeCertificate(lock.label, lock.getBytes(libcdoc::Lock::CERT));
	case libcdoc::Lock::PUBLIC_KEY:
	case libcdoc::Lock::SERVER:
		if (!server_id.empty()) {
			return libcdoc::Recipient::makeServer(lock.label, lock.getBytes(libcdoc::Lock::RCPT_KEY), server_id);
		} else {
			libcdoc::Recipient::PKType rcpt_type = (lock.pk_type == libcdoc::Lock::RSA) ? libcdoc::Recipient::RSA : libcdoc::Recipient::ECC;
			return libcdoc::Recipient::makePublicKey(lock.label, lock.getBytes(libcdoc::Lock::RCPT_KEY), rcpt_type);
		}
	default:
		return {};
	}
}
