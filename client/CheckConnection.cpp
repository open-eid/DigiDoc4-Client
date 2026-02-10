/*
 * QDigiDocClient
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

#include "CheckConnection.h"

#include "Application.h"
#include "Common.h"

#include <QtCore/QCoreApplication>
#include <QtCore/QEventLoop>
#include <QtCore/QJsonArray>
#include <QtCore/QJsonValue>
#include <QtNetwork/QNetworkAccessManager>
#include <QtNetwork/QNetworkRequest>

bool CheckConnection::check(const QUrl &url)
{
	QNetworkAccessManager nam;
	QEventLoop e;
	QNetworkReply *reply = nam.head(QNetworkRequest(url));
	QObject::connect(reply, &QNetworkReply::sslErrors, reply,
		QOverload<const QList<QSslError> &>::of(&QNetworkReply::ignoreSslErrors));
	QObject::connect(reply, &QNetworkReply::finished, &e, &QEventLoop::quit);
	e.exec();
	m_error = reply->error();
	qtmessage = reply->errorString();
	return m_error == QNetworkReply::NoError;
}

QString CheckConnection::errorDetails() const { return qtmessage; }
QString CheckConnection::errorString() const
{
	switch(m_error)
	{
	case QNetworkReply::NoError: return {};
	case QNetworkReply::ProxyConnectionRefusedError:
	case QNetworkReply::ProxyConnectionClosedError:
	case QNetworkReply::ProxyNotFoundError:
	case QNetworkReply::ProxyTimeoutError:
		return QCoreApplication::translate("CheckConnection", "Check proxy settings");
	case QNetworkReply::ProxyAuthenticationRequiredError:
		return QCoreApplication::translate("CheckConnection", "Check proxy username and password");
	default:
		return QCoreApplication::translate("CheckConnection", "Cannot connect to certificate status service!");
	}
}

QNetworkAccessManager* CheckConnection::setupNAM(QNetworkRequest &req, const QByteArray &add)
{
	req.setSslConfiguration(sslConfiguration(add));
	req.setRawHeader("User-Agent", QStringLiteral("%1/%2 (%3)")
		.arg(Application::applicationName(), Application::applicationVersion(), Common::applicationOs()).toUtf8());
	auto *nam = new QNetworkAccessManager();
	QObject::connect(nam, &QNetworkAccessManager::sslErrors, nam, [](QNetworkReply *reply, const QList<QSslError> &errors) {
		QList<QSslError> ignore;
		for(const QSslError &error: errors)
		{
			switch(error.error())
			{
			case QSslError::UnableToVerifyFirstCertificate:
			case QSslError::UnableToGetLocalIssuerCertificate:
			case QSslError::CertificateUntrusted:
			case QSslError::SelfSignedCertificateInChain:
				if(reply->sslConfiguration().caCertificates().contains(reply->sslConfiguration().peerCertificate())) {
					ignore.append(error);
					break;
				}
				Q_FALLTHROUGH();
			default:
				qDebug() << "SSL Error:" << error.error() << error.certificate().subjectInfo(QSslCertificate::CommonName);
			}
		}
		reply->ignoreSslErrors(ignore);
	});
	return nam;
}

QNetworkAccessManager* CheckConnection::setupNAM(QNetworkRequest &req,
	const QSslCertificate &cert, const QSslKey &key, const QByteArray &add)
{
	QNetworkAccessManager* nam = setupNAM(req, add);
	QSslConfiguration ssl = req.sslConfiguration();
	ssl.setPrivateKey(key);
	ssl.setLocalCertificate(cert);
	req.setSslConfiguration(ssl);
	return nam;
}

QSslConfiguration CheckConnection::sslConfiguration(const QByteArray &add)
{
	QSslConfiguration ssl = QSslConfiguration::defaultConfiguration();
#ifdef CONFIG_URL
	const auto list = Application::confValue(QLatin1String("CERT-BUNDLE")).toArray();
	QList<QSslCertificate> trusted;
	trusted.reserve(list.size());
	for(const auto &cert: list)
		trusted.append(QSslCertificate(QByteArray::fromBase64(cert.toString().toLatin1()), QSsl::Der));
	if(!add.isEmpty())
		trusted.append(QSslCertificate(QByteArray::fromBase64(add), QSsl::Der));
	ssl.setCaCertificates(trusted);
#endif
	return ssl;
}
