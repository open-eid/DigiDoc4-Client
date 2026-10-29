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
#include <QtCore/QDeadlineTimer>
#include <QtCore/QJsonArray>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <QtCore/QSettings>
#include <QtCore/QtEndian>
#include <QtCore/QTemporaryFile>
#include <QtCore/QThread>
#include <QtCore/QTimer>
#include <QtCore/QUrlQuery>
#include <QtNetwork/QNetworkAccessManager>
#include <QtNetwork/QSslKey>
#include <QLoggingCategory>
#include <QXmlStreamReader>
#include <algorithm>
#include <utility>

#include "Application.h"
#include "CheckConnection.h"
#include "QCryptoBackend.h"
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
	if(xml.hasError()) {
		// Malformed container, whatever was collected so far is incomplete
		qWarning() << "Failed to parse" << filename << ":" << xml.errorString();
		files.clear();
	}

	return files;
}

static libcdoc::result_t
getDecryptStatus(QCryptoBackend::Status pin_status)
{
	switch (pin_status) {
	case QCryptoBackend::PinOK:
		return libcdoc::OK;
	case QCryptoBackend::PinCanceled:
		return DDCryptoBackend::PIN_CANCELED;
	case QCryptoBackend::PinIncorrect:
		return DDCryptoBackend::PIN_INCORRECT;
	case QCryptoBackend::PinLocked:
		return DDCryptoBackend::PIN_LOCKED;
	case QCryptoBackend::InProgress:
		return DDCryptoBackend::IN_PROGRESS;
	default:
		return DDCryptoBackend::BACKEND_ERROR;
	}
}

static libcdoc::result_t
getDecryptResultStatus(const std::vector<uint8_t> &result, std::unique_ptr<QCryptoBackend> backend)
{
	if (!result.empty())
		return libcdoc::OK;
	libcdoc::result_t mapped = getDecryptStatus(backend->status);
	return mapped == libcdoc::OK ? DDCryptoBackend::BACKEND_ERROR : mapped;
}

libcdoc::result_t
DDCryptoBackend::decryptRSA(std::vector<uint8_t>& dst, const std::vector<uint8_t> &data, bool oaep, unsigned int idx)
{
	if (!backend)
		return BACKEND_ERROR;
	QByteArray decryptedKey = backend->decrypt(toByteArray(data), oaep);
	dst.assign(decryptedKey.cbegin(), decryptedKey.cend());
	return getDecryptResultStatus(dst, std::move(backend));
}

libcdoc::result_t
DDCryptoBackend::deriveConcatKDF(std::vector<uint8_t>& dst, const std::vector<uint8_t> &publicKey, const std::string &digest,
								 const std::vector<uint8_t> &algorithmID, const std::vector<uint8_t> &partyUInfo, const std::vector<uint8_t> &partyVInfo, unsigned int idx)
{
	static const QHash<std::string_view, QCryptographicHash::Algorithm> SHA_MTH{
		{"http://www.w3.org/2001/04/xmlenc#sha256", QCryptographicHash::Sha256},
		{"http://www.w3.org/2001/04/xmlenc#sha384", QCryptographicHash::Sha384},
		{"http://www.w3.org/2001/04/xmlenc#sha512", QCryptographicHash::Sha512}
	};
	if (!backend)
		return BACKEND_ERROR;
	QByteArray decryptedKey = backend->deriveConcatKDF(toByteArray(publicKey), SHA_MTH.value(digest),
		toByteArray(algorithmID), toByteArray(partyUInfo), toByteArray(partyVInfo));
	dst.assign(decryptedKey.cbegin(), decryptedKey.cend());
	return getDecryptResultStatus(dst, std::move(backend));
}

libcdoc::result_t
DDCryptoBackend::deriveHMACExtract(std::vector<uint8_t>& dst, const std::vector<uint8_t> &key_material, const std::vector<uint8_t> &salt, unsigned int idx)
{
	if (!backend)
		return BACKEND_ERROR;
	QByteArray decryptedKey = backend->deriveHMACExtract(toByteArray(key_material), toByteArray(salt), ECC_KEY_LEN);
	dst.assign(decryptedKey.cbegin(), decryptedKey.cend());
	return getDecryptResultStatus(dst, std::move(backend));
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
		case IN_PROGRESS:
			return "Signing/decrypting is already in progress another window.";
		case BACKEND_ERROR:
			return "Backend error";
	}
	return libcdoc::CryptoBackend::getLastErrorStr(code);
}

static bool
checkConnection()
{
	CheckConnection check;
	if(check.check()) {
		return true;
	}
	return dispatchToMain([check] {
		FadeInNotification::error(Application::mainWindow()->findChild<QWidget*>(QStringLiteral("content")),
								  check.errorString());
		return false;
	});
}

static QString
trimUrl(QString url)
{
	while (url.endsWith(QLatin1Char('/')))
		url.chop(1);
	return url;
}

// Mobile-ID / Smart-ID authentication (AUTH) or relying party (RP) server, unless
// overridden in the user settings from the CDOC2-CONF entry whose SHARES include
// all of the lock's share servers. Another entry's servers would not authorize these
// share servers, and would learn the user's identity only to fail after the login.
static std::string
shareServer(QLatin1String key, QLatin1String field, const std::string &shareUrls)
{
	if (QString url = QSettings().value(key).toString(); !url.isEmpty())
		return url.toStdString();
	QStringList lockUrls;
	for (const QString &share : QString::fromStdString(shareUrls).split(QLatin1Char(';'), Qt::SkipEmptyParts))
		lockUrls.append(trimUrl(share.section(QLatin1Char(','), 0, 0)));
	QJsonObject list = Application::confValue(QLatin1String("CDOC2-CONF")).toObject();
	for (const QJsonValue &value : list) {
		QJsonObject data = value.toObject();
		QStringList shares;
		for (const QJsonValue &url : data.value(QLatin1String("SHARES")).toArray())
			shares.append(trimUrl(url.toString()));
		if (!lockUrls.isEmpty() && std::all_of(lockUrls.cbegin(), lockUrls.cend(),
				[&shares](const QString &url) { return shares.contains(url); }))
			return data.value(field).toString().toStdString();
	}
	return {};
}

bool
DDConfiguration::shareServersKnown(const std::string &shareUrls)
{
	return !shareServer(QLatin1String("CDOC2-SHARES-AUTH"), QLatin1String("AUTH"), shareUrls).empty() &&
		!shareServer(QLatin1String("CDOC2-SHARES-RP"), QLatin1String("RP"), shareUrls).empty();
}

std::string
DDConfiguration::defaultShareServer()
{
	QString id = Settings::CDOC2_DEFAULT_KEYSERVER;
	QJsonObject data = Application::confValue(QLatin1String("CDOC2-CONF")).toObject().value(id).toObject();
	return data.value(QLatin1String("SHARES")).toArray().size() >= 2 ? id.toStdString() : std::string();
}

std::string
DDConfiguration::getValue(std::string_view domain, std::string_view param) const
{
	if (param == libcdoc::Configuration::AUTH_SERVER) {
		return shareServer(QLatin1String("CDOC2-SHARES-AUTH"), QLatin1String("AUTH"), shareUrls);
	} else if (param == libcdoc::Configuration::RP_SERVER) {
		return shareServer(QLatin1String("CDOC2-SHARES-RP"), QLatin1String("RP"), shareUrls);
	} else if (param == libcdoc::Configuration::SHARE_SIGNER) {
		return shareSigner;
	} else if (param == libcdoc::Configuration::PHONE_NUMBER) {
		return phone;
	} else if (param == libcdoc::Configuration::SHARE_SERVER_URLS) {
		QJsonObject list = Application::confValue(QLatin1String("CDOC2-CONF")).toObject();
		QJsonObject data = list.value(QLatin1String(domain.data(), domain.size())).toObject();
		if (QJsonArray shares = data.value(QLatin1String("SHARES")).toArray(); !shares.isEmpty())
			return QJsonDocument(shares).toJson(QJsonDocument::Compact).toStdString();
	} else if (param == libcdoc::Configuration::KEYSERVER_SEND_URL) {
#ifdef CONFIG_URL
		QJsonObject list = Application::confValue(QLatin1String("CDOC2-CONF")).toObject();
		QJsonObject data = list.value(QLatin1String(domain.data(), domain.size())).toObject();
		if (QString url = data.value(QLatin1String("POST")).toString(); !url.isEmpty())
			return url.toStdString();
		if (!domain.empty() && domain == Settings::CDOC2_UUID) {
			QString url = Settings::CDOC2_POST;
			return url.toStdString();
		}
#else
		if (domain == Settings::CDOC2_DEFAULT_KEYSERVER) {
			QString url = Settings::CDOC2_POST;
			return url.toStdString();
		}
#endif
	} else if (param == libcdoc::Configuration::KEYSERVER_FETCH_URL) {
#ifdef CONFIG_URL
		QJsonObject list = Application::confValue(QLatin1String("CDOC2-CONF")).toObject();
		QJsonObject data = list.value(QLatin1String(domain.data(), domain.size())).toObject();
		if (QString url = data.value(QLatin1String("FETCH")).toString(); !url.isEmpty())
			return url.toStdString();
		if (!domain.empty() && domain == Settings::CDOC2_UUID) {
			QString url = Settings::CDOC2_GET;
			return url.toStdString();
		}
#else
		if (domain == Settings::CDOC2_DEFAULT_KEYSERVER) {
			QString url = Settings::CDOC2_GET;
			return url.toStdString();
		}
#endif
	}
	return {};
}

std::string
DDNetworkBackend::getLastErrorStr(libcdoc::result_t code) const
{
	switch (code) {
		case DDCryptoBackend::PIN_CANCELED:
			return "PIN entry canceled";
		case DDCryptoBackend::PIN_INCORRECT:
			return "PIN incorrect";
		case DDCryptoBackend::PIN_LOCKED:
			return "PIN locked";
		case DDCryptoBackend::IN_PROGRESS:
			return "Signing/decrypting is already in progress another window.";
		case BACKEND_ERROR:
		case DDCryptoBackend::BACKEND_ERROR:
			return last_error;
	}
	return libcdoc::NetworkBackend::getLastErrorStr(code);
}

static std::string
replyError(QNetworkReply *reply)
{
	QString error = reply->errorString();
	QJsonObject problem = QJsonDocument::fromJson(reply->readAll()).object();
	QString detail = problem.value(QLatin1String("errorMessage")).toString();
	if (detail.isEmpty())
		detail = problem.value(QLatin1String("errorCode")).toString();
	if (!detail.isEmpty())
		error += QLatin1String(": ") + detail;
	return error.toStdString();
}

libcdoc::result_t DDNetworkBackend::sendKey(
	libcdoc::NetworkBackend::CapsuleInfo &dst, const std::string &url,
	const std::vector<uint8_t> &rcpt_key,
	const std::vector<uint8_t> &key_material, const std::string &type, uint64_t expiry_ts) {
	QNetworkRequest req(QString::fromStdString(url + "/key-capsules"));
	req.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
	if (expiry_ts) {
		uint64_t expiry = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now() + std::chrono::months(6));
		expiry_ts = std::min(expiry_ts, expiry);
		req.setRawHeader("x-expiry-time", QDateTime::fromSecsSinceEpoch(expiry_ts, Qt::UTC).toString(Qt::ISODate).toLatin1());
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
		last_error = replyError(reply);
		return BACKEND_ERROR;
	}
	int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
	if (status != 201) {
		last_error = replyError(reply);
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
DDNetworkBackend::fetchKey(std::vector<uint8_t> &result, const std::string &url, const std::string &transaction_id)
{
	QNetworkRequest req(QStringLiteral("%1/key-capsules/%2").arg(QString::fromStdString(url), QLatin1String(transaction_id.c_str())));
	req.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
	if(!checkConnection()) {
		last_error = "No connection";
		return BACKEND_ERROR;
	}

	if (!crypto.backend) {
		last_error = "No authentication backend";
		return BACKEND_ERROR;
	}
	auto authKey = crypto.backend->getKey();
	if (!authKey.handle()) {
		last_error = "Cannot create authentication key";
		return BACKEND_ERROR;
	}
	QScopedPointer<QNetworkAccessManager,QScopedPointerDeleteLater> nam(
				CheckConnection::setupNAM(req, crypto.backend->cert(), authKey, Settings::CDOC2_GET_CERT));
	QEventLoop e;
	QNetworkReply *reply = nam->get(req);
	connect(reply, &QNetworkReply::finished, &e, &QEventLoop::quit);
	e.exec();

	if(reply->error() != QNetworkReply::NoError && reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() != 201) {
		last_error = replyError(reply);
		return BACKEND_ERROR;
	}
	QJsonObject json = QJsonDocument::fromJson(reply->readAll()).object();
	QByteArray key_material = QByteArray::fromBase64(json.value(QLatin1String("ephemeral_key_material")).toString().toLatin1());
	result.assign(key_material.cbegin(), key_material.cend());

	return libcdoc::OK;
}

libcdoc::result_t
DDNetworkBackend::get(const std::string &url, std::vector<uint8_t> &body,
	std::map<std::string, std::string> &headers, bool client_cert)
{
	return request(false, url, body, headers, client_cert);
}

libcdoc::result_t
DDNetworkBackend::post(const std::string &url, std::vector<uint8_t> &body,
	std::map<std::string, std::string> &headers, bool client_cert)
{
	return request(true, url, body, headers, client_cert);
}

libcdoc::result_t
DDNetworkBackend::request(bool post, const std::string &url, std::vector<uint8_t> &body,
	std::map<std::string, std::string> &headers, bool client_cert)
{
	if (client_cert) {
		// Only the key server authenticates with the card, and fetchKey does that itself
		last_error = "Client certificate is not supported";
		return libcdoc::NOT_IMPLEMENTED;
	}
	if (!checkConnection()) {
		last_error = "No connection";
		return BACKEND_ERROR;
	}
	return send(post, QUrl(QString::fromStdString(url)), body, headers);
}

libcdoc::result_t
DDNetworkBackend::send(bool post, const QUrl &url, std::vector<uint8_t> &body,
	std::map<std::string, std::string> &headers, int timeout)
{
	if (canceled)
		return DDCryptoBackend::PIN_CANCELED;
	QNetworkRequest req(url);
	req.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
	for (const auto &[name, value] : headers)
		req.setRawHeader(QByteArray::fromStdString(name), QByteArray::fromStdString(value));
	QScopedPointer<QNetworkAccessManager, QScopedPointerDeleteLater> nam(CheckConnection::setupNAM(req));
	QNetworkReply *reply = post ? nam->post(req, toByteArray(body)) : nam->get(req);
	QEventLoop e;
	connect(reply, &QNetworkReply::finished, &e, &QEventLoop::quit);
	QDeadlineTimer deadline(timeout);
	QTimer timer;
	connect(&timer, &QTimer::timeout, reply, [&] {
		if(canceled || deadline.hasExpired())
			reply->abort();
	});
	timer.start(100);
	if(!reply->isFinished())
		e.exec();
	if(canceled)
		return DDCryptoBackend::PIN_CANCELED;
	if(deadline.hasExpired()) {
		last_error = "Timeout waiting for server response";
		return BACKEND_ERROR;
	}
	int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
	if (reply->error() != QNetworkReply::NoError || status < 200 || status >= 300) {
		last_error = replyError(reply);
		return BACKEND_ERROR;
	}
	// Callers look headers up in lower case, as libcdoc's own client stores them
	headers.clear();
	for (const auto &[name, value] : reply->rawHeaderPairs())
		headers.emplace(name.toLower().toStdString(), value.toStdString());
	QByteArray data = reply->readAll();
	body.assign(data.cbegin(), data.cend());
	return libcdoc::OK;
}

// The Mobile-ID / Smart-ID round-trips below follow libcdoc's own, which use its
// built-in HTTP client and so would need the servers' certificates pinned

libcdoc::result_t
DDNetworkBackend::getAuthResponse(const std::string &url, std::vector<uint8_t> &body,
	std::map<std::string, std::string> &headers)
{
	if (!checkConnection()) {
		last_error = "No connection";
		return BACKEND_ERROR;
	}
	QUrl start(QString::fromStdString(url));
	QString path = start.path();
	if (path.endsWith(QLatin1Char('/')))
		path.chop(1);
	start.setPath(path + QLatin1String("/auth/start"));
	if (auto result = send(true, start, body, headers); result != libcdoc::OK)
		return result;
	// The status is polled on the server the session was started on
	QString location = QString::fromStdString(headers["location"]);
	if (!location.startsWith(path + QLatin1String("/auth/status/"))) {
		last_error = "Unexpected Location header value";
		return BACKEND_ERROR;
	}
	bool ok = false;
	SIDMIDFeedback feedback { .code = QJsonDocument::fromJson(toByteArray(body)).object()
		.value(QLatin1String("vc")).toString().toInt(&ok) };
	if (!ok || feedback.code < 0 || feedback.code > 9999) {
		last_error = "Invalid verification code in response";
		return BACKEND_ERROR;
	}
	if (auto result = showFeedback(feedback); result != libcdoc::OK)
		return result;
	QUrl status = start;
	status.setPath(location);
	headers.clear();
	return poll(status, QLatin1String("status"), body, headers);
}

libcdoc::result_t
DDNetworkBackend::getSignResponse(const std::string &url, const std::string &post_path,
	std::vector<uint8_t> &body, std::map<std::string, std::string> &headers,
	const std::string &poll_path_prefix)
{
	if (!checkConnection()) {
		last_error = "No connection";
		return BACKEND_ERROR;
	}
	QUrl sign(QString::fromStdString(url));
	QString path = sign.path();
	if (path.endsWith(QLatin1Char('/')))
		path.chop(1);
	sign.setPath(path + QString::fromStdString(post_path));
	// The session token headers go with every poll as well
	const std::map<std::string, std::string> request_headers = headers;
	if (auto result = send(true, sign, body, headers); result != libcdoc::OK)
		return result;
	QString session = QJsonDocument::fromJson(toByteArray(body)).object()
		.value(QLatin1String("sessionID")).toString();
	if (session.isEmpty()) {
		last_error = "No session ID in response";
		return BACKEND_ERROR;
	}
	QUrl status = sign;
	status.setPath(path + QString::fromStdString(poll_path_prefix) + session);
	headers = request_headers;
	return poll(status, QLatin1String("state"), body, headers);
}

libcdoc::result_t
DDNetworkBackend::poll(const QUrl &url, QLatin1String field, std::vector<uint8_t> &body,
	std::map<std::string, std::string> &headers)
{
	const std::map<std::string, std::string> request_headers = headers;
	for (QDeadlineTimer deadline(std::chrono::minutes(1)); !deadline.hasExpired();) {
		headers = request_headers;
		if (auto result = send(false, url, body, headers,
			int(std::min<qint64>(30'000, deadline.remainingTime()))); result != libcdoc::OK)
			return result;
		QJsonObject json = QJsonDocument::fromJson(toByteArray(body)).object();
		QString state = json.value(field).toString();
		if (state == QLatin1String("COMPLETE"))
			return libcdoc::OK;
		if (state != QLatin1String("RUNNING") && state != QLatin1String("STARTED")) {
			// A session that ended without success says why in endResult (e.g. NOT_MID_CLIENT),
			// which the caller turns into its Mobile-ID / Smart-ID error
			if (json.contains(QLatin1String("endResult")))
				return libcdoc::OK;
			last_error = "Invalid Mobile-ID / Smart-ID state: " + state.toStdString();
			return BACKEND_ERROR;
		}
		QThread::msleep(500);
	}
	last_error = "Timeout waiting for Mobile-ID / Smart-ID";
	return BACKEND_ERROR;
}

libcdoc::result_t
DDNetworkBackend::showFeedback(SIDMIDFeedback &feedback)
{
	if (canceled)
		return DDCryptoBackend::PIN_CANCELED;
	if (onFeedback)
		QMetaObject::invokeMethod(qApp, [callback = onFeedback, code = feedback.code] { callback(code); });
	return libcdoc::OK;
}

static constexpr QLatin1String
level2Str(libcdoc::LogLevel level) noexcept
{
	switch (level) {
		using enum libcdoc::LogLevel;
	case LEVEL_FATAL: return QLatin1String("FATAL");
	case LEVEL_ERROR: return QLatin1String("ERROR");
	case LEVEL_WARNING: return QLatin1String("WARNING");
	case LEVEL_INFO: return QLatin1String("INFO");
	case LEVEL_DEBUG: return QLatin1String("DEBUG");
	case LEVEL_TRACE: return QLatin1String("TRACE");
	default: return QLatin1String("UNKNWON");
	}
}

void DDCDocLogger::logMessage(libcdoc::LogLevel level, std::string_view file, int line, std::string_view message) {
	if (!ofs.isOpen()) {
		return;
	}
	auto time = QDateTime::currentDateTime().toString(QStringLiteral("[yyyy-MM-dd hh:mm:ss]"));
	if (auto pos = file.find_last_of("\\/"); pos != std::string_view::npos)
		file = file.substr(pos);
 
	ofs.write(QStringLiteral("%1 %2 %3:%4 %5\n")
		.arg(time, level2Str(level), QLatin1String(file.data(), file.size()))
		.arg(line)
		.arg(QLatin1String(message.data(), message.size())).toUtf8());
}

DDCDocLogger *
DDCDocLogger::getLogger()
{
	static DDCDocLogger logger;
	return &logger;
}

void DDCDocLogger::setUpLogger(const QString &path)
{
	DDCDocLogger *logger = getLogger();
	logger->setMinLogLevel(libcdoc::LEVEL_WARNING);
	logger->ofs.setFileName(path);
	if(logger->ofs.open(QFile::WriteOnly|QFile::Append))
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
	if (auto result = file.data->write((const char *)src, size); std::cmp_not_equal(result , size))
		return result;
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
	IOEntry io({std::move(truncated), "application/octet-stream", 0, {}});
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
