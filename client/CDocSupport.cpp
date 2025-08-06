#define __CDOCSUPPORT_CPP__

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

#include <QtCore/QBuffer>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <QtCore/QtEndian>
#include <QtCore/QTemporaryFile>
#include <QtNetwork/QNetworkAccessManager>
#include <QtNetwork/QSslKey>
#include <QtCore/QJsonDocument>
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

#include "CDocSupport.h"

#if QT_VERSION < QT_VERSION_CHECK(6, 5, 0)
#define SV2S(m) QUtf8StringView(m)
#define Q_FATAL(m) QFatal("%s", std::string(m).c_str())
#else
#define SV2S(m) (m)
#define Q_FATAL(m) qCFatal(LOG_CDOC) << (m)
#endif

std::vector<libcdoc::FileInfo>
CDocSupport::getCDocFileList(QString filename)
{
	std::vector<libcdoc::FileInfo> files;
	int version = libcdoc::CDocReader::getCDocFileVersion(filename.toStdString());
	if (version != 1) return files;
	QFile ifs(filename);
	ifs.open(QIODevice::ReadOnly);
	QXmlStreamReader xml(&ifs);
	while (xml.readNextStartElement()) {
		if (xml.name() == QStringLiteral("EncryptedData")) {
			while (xml.readNextStartElement()) {
				if (xml.name() == QStringLiteral("EncryptionProperties")) {
					while (xml.readNextStartElement()) {
						if (xml.name() == QStringLiteral("EncryptionProperty")) {
							if (xml.attributes().value(QStringLiteral("Name")) == QStringLiteral("orig_file")) {
								QString content = xml.readElementText();
								auto list = content.split("|");
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
	ifs.close();

	return files;
}

libcdoc::result_t
DDCryptoBackend::decryptRSA(std::vector<uint8_t>& result, const std::vector<uint8_t> &data, bool oaep, unsigned int idx)
{
	QByteArray qdata(reinterpret_cast<const char *>(data.data()), data.size());
	QByteArray qkek = qApp->signer()->decrypt([&qdata, &oaep](QCryptoBackend *backend) {
			return backend->decrypt(qdata, oaep);
	});
	result.assign(qkek.cbegin(), qkek.cend());
	return (result.empty()) ? OPENSSL_ERROR : libcdoc::OK;
}

const QString SHA256_MTH = QStringLiteral("http://www.w3.org/2001/04/xmlenc#sha256");
const QString SHA384_MTH = QStringLiteral("http://www.w3.org/2001/04/xmlenc#sha384");
const QString SHA512_MTH = QStringLiteral("http://www.w3.org/2001/04/xmlenc#sha512");
const QHash<QString, QCryptographicHash::Algorithm> SHA_MTH{
	{SHA256_MTH, QCryptographicHash::Sha256}, {SHA384_MTH, QCryptographicHash::Sha384}, {SHA512_MTH, QCryptographicHash::Sha512}
};

libcdoc::result_t
DDCryptoBackend::deriveConcatKDF(std::vector<uint8_t>& dst, const std::vector<uint8_t> &publicKey, const std::string &digest,
								 const std::vector<uint8_t> &algorithmID, const std::vector<uint8_t> &partyUInfo, const std::vector<uint8_t> &partyVInfo, unsigned int idx)
{
	QByteArray decryptedKey = qApp->signer()->decrypt([&publicKey, &digest, &algorithmID, &partyUInfo, &partyVInfo](QCryptoBackend *backend) {
			QByteArray ba(reinterpret_cast<const char *>(publicKey.data()), publicKey.size());
			return backend->deriveConcatKDF(ba, SHA_MTH[QString::fromStdString(digest)],
				QByteArray(reinterpret_cast<const char *>(algorithmID.data()), algorithmID.size()),
				QByteArray(reinterpret_cast<const char *>(partyUInfo.data()), partyUInfo.size()),
				QByteArray(reinterpret_cast<const char *>(partyVInfo.data()), partyVInfo.size()));
	});
	dst.assign(decryptedKey.cbegin(), decryptedKey.cend());
	return (dst.empty()) ? OPENSSL_ERROR : libcdoc::OK;
}

libcdoc::result_t
DDCryptoBackend::deriveHMACExtract(std::vector<uint8_t>& dst, const std::vector<uint8_t> &key_material, const std::vector<uint8_t> &salt, unsigned int idx)
{
	QByteArray qkey_material(reinterpret_cast<const char *>(key_material.data()), key_material.size());
	QByteArray qsalt(reinterpret_cast<const char *>(salt.data()), salt.size());
	QByteArray qkekpm = qApp->signer()->decrypt([&qkey_material, &qsalt](QCryptoBackend *backend) {
		return backend->deriveHMACExtract(qkey_material, qsalt, ECC_KEY_LEN);
	});
	dst = std::vector<uint8_t>(qkekpm.cbegin(), qkekpm.cend());
	return (dst.empty()) ? OPENSSL_ERROR : libcdoc::OK;
}

libcdoc::result_t
DDCryptoBackend::getSecret(std::vector<uint8_t>& _secret, unsigned int idx)
{
	_secret = secret;
	return libcdoc::OK;
}

bool
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
	if (domain == def) {
		if (param == libcdoc::Configuration::KEYSERVER_SEND_URL) {
#ifdef CONFIG_URL
			QJsonObject list = Application::confValue(QLatin1String("CDOC2-CONF")).toObject();
			QJsonObject data = list.value(QString::fromUtf8(domain.data(), domain.size())).toObject();
			QString url = data.value(QLatin1String("POST")).toString(Settings::CDOC2_POST);
			return url.toStdString();
#else
			QString url = Settings::CDOC2_POST;
			return url.toStdString();
#endif
		} else if (param == libcdoc::Configuration::KEYSERVER_FETCH_URL) {
#ifdef CONFIG_URL
			QJsonObject list = Application::confValue(QLatin1String("CDOC2-CONF")).toObject();
			QJsonObject data = list.value(QString::fromUtf8(domain.data(), domain.size())).toObject();
			QString url = data.value(QLatin1String("FETCH")).toString(Settings::CDOC2_GET);
			return url.toStdString();
#else
			QString url = Settings::CDOC2_GET;
			return url.toStdString();
#endif
		}
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
	const std::vector<uint8_t> &key_material, const std::string &type) {
	QNetworkRequest req(QString::fromStdString(url + "/key-capsules"));
	req.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
	if (!checkConnection()) {
		last_error = "No connection";
		return BACKEND_ERROR;
	}
	QScopedPointer<QNetworkAccessManager, QScopedPointerDeleteLater> nam(CheckConnection::setupNAM(req, Settings::CDOC2_POST_CERT));
	QEventLoop e;
	QNetworkReply *reply = nam->post(req,
		QJsonDocument({
				{QLatin1String("recipient_id"),
				 QLatin1String(
					 QByteArray(reinterpret_cast<const char *>(rcpt_key.data()),
								rcpt_key.size())
						 .toBase64())},
				{QLatin1String("ephemeral_key_material"),
				 QLatin1String(QByteArray(reinterpret_cast<const char *>(
											  key_material.data()),
										  key_material.size())
								   .toBase64())},
				{QLatin1String("capsule_type"), QLatin1String(type.c_str())},
			}).toJson());
	connect(reply, &QNetworkReply::finished, &e, &QEventLoop::quit);
	e.exec();
	QString tr_id;
	if (reply->error() == QNetworkReply::NoError &&
		reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() ==
			201) {
		tr_id = QString::fromLatin1(reply->rawHeader("Location"))
					.remove(QLatin1String("/key-capsules/"));
	} else {
		last_error = reply->errorString().toStdString();
		return BACKEND_ERROR;
	}
	if (tr_id.isEmpty()) {
		last_error = "Failed to post key capsule";
		return BACKEND_ERROR;
	}
	dst.transaction_id = tr_id.toStdString();
	QDateTime dt = QDateTime::currentDateTimeUtc();
	dt = dt.addMonths(6);
	dst.expiry_time = dt.toSecsSinceEpoch();
	return libcdoc::OK;
};

libcdoc::result_t
DDNetworkBackend::fetchKey(std::vector<uint8_t> &result,
						   const std::string &url,
						   const std::string &transaction_id) {
	QNetworkRequest req(QStringLiteral("%1/key-capsules/%2").arg(QString::fromStdString(url), QString::fromStdString(transaction_id)));
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

Q_DECLARE_LOGGING_CATEGORY(LOG_CDOC)
Q_LOGGING_CATEGORY(LOG_CDOC, "libcdoc")

void DDCDocLogger::LogMessage(libcdoc::ILogger::LogLevel level, std::string_view file, int line, std::string_view message) {
	switch (level) {
	case libcdoc::ILogger::LogLevel::LEVEL_FATAL:
		Q_FATAL(message);
		break;
	case libcdoc::ILogger::LogLevel::LEVEL_ERROR:
		qCCritical(LOG_CDOC) << SV2S(message);
		break;
	case libcdoc::ILogger::LogLevel::LEVEL_WARNING:
		qCWarning(LOG_CDOC) << SV2S(message);
		break;
	case libcdoc::ILogger::LogLevel::LEVEL_INFO:
		qCInfo(LOG_CDOC) << SV2S(message);
		break;
	case libcdoc::ILogger::LogLevel::LEVEL_DEBUG:
		qCDebug(LOG_CDOC) << SV2S(message);
		break;
	default:
		// Trace, if present goes to debug categrory
		qCDebug(LOG_CDOC) << SV2S(message);
		break;
	}
}

void DDCDocLogger::setUpLogger() {
	static DDCDocLogger *logger = nullptr;
	if (logger) {
		logger = new DDCDocLogger();
		logger->SetMinLogLevel(libcdoc::ILogger::LogLevel::LEVEL_TRACE);
		libcdoc::ILogger::addLogger(logger);
	}
}

TempListConsumer::~TempListConsumer()
{
	if (!files.empty()) {
		IOEntry& file = files.back();
		file.data->close();
	}
}

libcdoc::result_t TempListConsumer::write(const uint8_t *src, size_t size) {
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

libcdoc::result_t TempListConsumer::close() {
	if (files.empty())
		return libcdoc::OUTPUT_ERROR;
	IOEntry &file = files.back();
	if (!file.data->isWritable())
		return libcdoc::OUTPUT_ERROR;
	return libcdoc::OK;
}

bool
TempListConsumer::isError()
{
	if (files.empty()) return false;
	IOEntry& file = files.back();
	return !file.data->isWritable();
}

libcdoc::result_t
TempListConsumer::open(const std::string& name, int64_t size)
{
	IOEntry io({name, "application/octet-stream", 0, {}});
	if ((size < 0) || (size > MAX_VEC_SIZE)) {
		io.data = std::make_unique<QTemporaryFile>();
	} else {
		io.data = std::make_unique<QBuffer>();
	}
	io.data->open(QIODevice::ReadWrite);
	files.push_back(std::move(io));
	return libcdoc::OK;
}

StreamListSource::StreamListSource(const std::vector<IOEntry>& files) : _files(files), _current(-1)
{
}

libcdoc::result_t
StreamListSource::read(uint8_t *dst, size_t size)
{
	if ((_current < 0) || (_current >= _files.size())) return 0;
	return _files[_current].data->read((char *) dst, size);
}

bool
StreamListSource::isError()
{
	if ((_current < 0) || (_current >= _files.size())) return 0;
	return _files[_current].data->isReadable();
}

bool
StreamListSource::isEof()
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
