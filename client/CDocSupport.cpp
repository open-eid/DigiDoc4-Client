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

#include "Application.h"
#include "CheckConnection.h"
#include "Colors.h"
#include "QCryptoBackend.h"
#include "QSigner.h"
#include "Settings.h"
#include "TokenData.h"
#include "Utils.h"
#include "effects/FadeInNotification.h"

#include "CDocSupport.h"

int
DDCryptoBackend::decryptRSA(std::vector<uint8_t>& result, const std::vector<uint8_t> &data, bool oaep) const
{
	QByteArray qdata(reinterpret_cast<const char *>(data.data()), data.size());
	QByteArray qkek = qApp->signer()->decrypt([&qdata, &oaep](QCryptoBackend *backend) {
			return backend->decrypt(qdata, oaep);
	});
	result.assign(qkek.cbegin(), qkek.cend());
	return (result.empty()) ? OPENSSL_ERROR : OK;
}

const QString SHA256_MTH = QStringLiteral("http://www.w3.org/2001/04/xmlenc#sha256");
const QString SHA384_MTH = QStringLiteral("http://www.w3.org/2001/04/xmlenc#sha384");
const QString SHA512_MTH = QStringLiteral("http://www.w3.org/2001/04/xmlenc#sha512");
const QHash<QString, QCryptographicHash::Algorithm> SHA_MTH{
	{SHA256_MTH, QCryptographicHash::Sha256}, {SHA384_MTH, QCryptographicHash::Sha384}, {SHA512_MTH, QCryptographicHash::Sha512}
};

int
DDCryptoBackend::deriveConcatKDF(std::vector<uint8_t>& dst, const std::vector<uint8_t> &publicKey, const std::string &digest, int keySize,
	const std::vector<uint8_t> &algorithmID, const std::vector<uint8_t> &partyUInfo, const std::vector<uint8_t> &partyVInfo)
{
	QByteArray decryptedKey = qApp->signer()->decrypt([&publicKey, &digest, &keySize, &algorithmID, &partyUInfo, &partyVInfo](QCryptoBackend *backend) {
			QByteArray ba(reinterpret_cast<const char *>(publicKey.data()), publicKey.size());
			return backend->deriveConcatKDF(ba, SHA_MTH[QString::fromStdString(digest)],
				keySize,
				QByteArray(reinterpret_cast<const char *>(algorithmID.data()), algorithmID.size()),
				QByteArray(reinterpret_cast<const char *>(partyUInfo.data()), partyUInfo.size()),
				QByteArray(reinterpret_cast<const char *>(partyVInfo.data()), partyVInfo.size()));
	});
	dst.assign(decryptedKey.cbegin(), decryptedKey.cend());
	return (dst.empty()) ? OPENSSL_ERROR : OK;
}

std::vector<uint8_t>
DDCryptoBackend::deriveHMACExtract(const std::vector<uint8_t> &key_material, const std::vector<uint8_t> &salt, int keySize) const
{
	QByteArray qkey_material(reinterpret_cast<const char *>(key_material.data()), key_material.size());
	QByteArray qsalt(reinterpret_cast<const char *>(salt.data()), salt.size());
	QByteArray qkekpm = qApp->signer()->decrypt([&qkey_material, &qsalt, &keySize](QCryptoBackend *backend) {
		return backend->deriveHMACExtract(qkey_material, qsalt, keySize);
	});
	return std::vector<uint8_t>(qkekpm.cbegin(), qkekpm.cend());
}

int
DDCryptoBackend::getSecret(std::vector<uint8_t>& _secret, const std::string& _label)
{
	_secret = secret;
	return OK;
}

bool
checkConnection()
{
	if(CheckConnection().check()){
		return true;
	}
	return dispatchToMain([] {
		auto *notification = new FadeInNotification(Application::mainWindow(), ria::qdigidoc4::colors::WHITE, ria::qdigidoc4::colors::MANTIS, 110);
		notification->start(QCoreApplication::translate("MainWindow", "Check internet connection"), 750, 3000, 1200);
		return false;
	});
}

QNetworkRequest
request(const QString &keyserver_id, const QString &transaction_id = {}) {
#ifdef CONFIG_URL
	QJsonObject list = Application::confValue(QLatin1String("CDOC2-CONF")).toObject();
	QJsonObject data = list.value(keyserver_id).toObject();
	QString url = transaction_id.isEmpty() ?
		data.value(QLatin1String("POST")).toString(Settings::CDOC2_POST) :
		data.value(QLatin1String("FETCH")).toString(Settings::CDOC2_GET);
#else
	QString url = transaction_id.isEmpty() ? Settings::CDOC2_POST : Settings::CDOC2_GET;
#endif
	if(url.isEmpty())
		return QNetworkRequest{};
	QNetworkRequest req(QStringLiteral("%1/key-capsules%2").arg(url,
		transaction_id.isEmpty() ? QString(): QStringLiteral("/%1").arg(transaction_id)));
	req.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
	return req;
}

std::string
DDConfiguration::getValue(const std::string& param)
{
	if (param == libcdoc::Configuration::USE_KEYSERVER) {
		return (Settings::CDOC2_USE_KEYSERVER) ? "true" : "false";
	}
	return {};
}

std::pair<std::string,std::string>
DDNetworkBackend::sendKey (libcdoc::CDocWriter *writer, const std::vector<uint8_t> &recipient_id, const std::vector<uint8_t> &key_material, const std::string &type)
{
	std::string keyserver_id = Settings::CDOC2_DEFAULT_KEYSERVER;
	if(keyserver_id.empty()) {
		writer->setLastError(t_("keyserver_id cannot be empty"));
		return {};
	}
	QNetworkRequest req = request(QString::fromStdString(keyserver_id));
	if(req.url().isEmpty()) {
		writer->setLastError(t_("No valid config found for keyserver_id: ") + keyserver_id);
		return {};
	}
	if(!checkConnection()) {
		return {};
	}
	QScopedPointer<QNetworkAccessManager,QScopedPointerDeleteLater> nam(CheckConnection::setupNAM(req, Settings::CDOC2_POST_CERT));
	QEventLoop e;
	QNetworkReply *reply = nam->post(req, QJsonDocument({
		{QLatin1String("recipient_id"), QLatin1String(QByteArray(reinterpret_cast<const char *>(recipient_id.data()), recipient_id.size()).toBase64())},
		{QLatin1String("ephemeral_key_material"), QLatin1String(QByteArray(reinterpret_cast<const char *>(key_material.data()), key_material.size()).toBase64())},
		{QLatin1String("capsule_type"), QLatin1String(type)},
	}).toJson());
	connect(reply, &QNetworkReply::finished, &e, &QEventLoop::quit);
	e.exec();
	QString transaction_id;
	if(reply->error() == QNetworkReply::NoError &&
		reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() == 201) {
		transaction_id = QString::fromLatin1(reply->rawHeader("Location")).remove(QLatin1String("/key-capsules/"));
	} else {
		writer->setLastError(reply->errorString().toStdString());
		return {};
	}
	if(transaction_id.isEmpty())
		writer->setLastError(t_("Failed to post key capsule"));
	return {keyserver_id, transaction_id.toStdString()};
};

std::vector<uint8_t>
DDNetworkBackend::fetchKey(libcdoc::CDocReader *reader, const libcdoc::CKeyServer& key)
{
	QNetworkRequest req = request(QString::fromStdString(key.keyserver_id), QString::fromStdString(key.transaction_id));
	if(req.url().isEmpty()) {
		reader->setLastError(t_("No valid config found for keyserver_id:") + key.keyserver_id);
		return {};
	}
	if(!checkConnection()) {
		return {};
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
		reader->setLastError(reply->errorString().toStdString());
		return {};
	}
	QJsonObject json = QJsonDocument::fromJson(reply->readAll()).object();
	QByteArray key_material = QByteArray::fromBase64(json.value(QLatin1String("ephemeral_key_material")).toString().toLatin1());
	return std::vector<uint8_t>(key_material.cbegin(), key_material.cend());
}

TempListConsumer::~TempListConsumer()
{
	if (!files.empty()) {
		IOEntry& file = files.back();
		file.data->close();
	}
}

int64_t
TempListConsumer::write(const uint8_t *src, size_t size)
{
	if (files.empty()) return OUTPUT_ERROR;
	IOEntry& file = files.back();
	if (!file.data->isWritable()) return OUTPUT_ERROR;
	if (file.data->write((const char *) src, size) != size) return OUTPUT_STREAM_ERROR;
	file.size += size;
	return size;
}

int
TempListConsumer::close()
{
	if (files.empty()) return OUTPUT_ERROR;
	IOEntry& file = files.back();
	if (!file.data->isWritable()) return OUTPUT_ERROR;
	return OK;
}

bool
TempListConsumer::isError()
{
	if (files.empty()) return false;
	IOEntry& file = files.back();
	return !file.data->isWritable();
}

bool
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
	return true;
}

StreamListSource::StreamListSource(const std::vector<IOEntry>& files) : _files(files), _current(-1)
{
}

int64_t
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

size_t
StreamListSource::getNumComponents()
{
	return _files.size();
}

bool
StreamListSource::next(std::string& name, int64_t& size)
{
	++_current;
	if (_current >= _files.size()) return false;
	name = _files[_current].name;
	size = _files[_current].size;
	return true;
}
