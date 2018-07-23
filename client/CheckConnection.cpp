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

#include <QtCore/QEventLoop>
#include <QtNetwork/QNetworkRequest>

CheckConnection::CheckConnection( QObject *parent )
:	QNetworkAccessManager( parent )
,	m_error( QNetworkReply::NoError )
{}

bool CheckConnection::check( const QString &url )
{
	QEventLoop e;
	QNetworkReply *reply = head(QNetworkRequest(url));
	connect(reply, &QNetworkReply::sslErrors, reply, [&](const QList<QSslError> &errors){
		reply->ignoreSslErrors(errors);
	});
	connect(reply, &QNetworkReply::finished, &e, &QEventLoop::quit);
	e.exec();
	m_error = reply->error();
	qtmessage = reply->errorString();
	reply->deleteLater();
	return m_error == QNetworkReply::NoError;
}

QNetworkReply::NetworkError CheckConnection::error() const { return m_error; }
QString CheckConnection::errorDetails() const { return qtmessage; }
QString CheckConnection::errorString() const
{
	switch(m_error)
	{
	case QNetworkReply::NoError: return QString();
	case QNetworkReply::ProxyConnectionRefusedError:
	case QNetworkReply::ProxyConnectionClosedError:
	case QNetworkReply::ProxyNotFoundError:
	case QNetworkReply::ProxyTimeoutError:
		return tr("%1, cannot connect to certificate status service!").arg(tr("Check proxy settings"));
	case QNetworkReply::ProxyAuthenticationRequiredError:
		return tr("%1, cannot connect to certificate status service!").arg(tr("Check proxy username and password"));
	default:
		return tr("Cannot connect to certificate status service!");
	}
}
