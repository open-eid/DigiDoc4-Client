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

#include <common/Common.h>

#include <QtCore/QEventLoop>
#include <QtNetwork/QNetworkRequest>

CheckConnection::CheckConnection( QObject *parent )
:	QNetworkAccessManager( parent )
,	m_error( QNetworkReply::NoError )
{}

bool CheckConnection::check( const QString &url )
{
	QEventLoop e;
	QNetworkRequest req( url );
#if 0
	req.setRawHeader( "User-Agent", QString( "%1/%2 (%3)")
		.arg( qApp->applicationName() ).arg( qApp->applicationVersion() ).arg( Common::applicationOs() ).toUtf8() );
#endif
	QNetworkReply *reply = get( req );
	connect( reply, SIGNAL(finished()), &e, SLOT(quit()) );
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
	switch( m_error )
	{
	case QNetworkReply::NoError: return QString();
	case QNetworkReply::ProxyConnectionRefusedError:
	case QNetworkReply::ProxyConnectionClosedError:
	case QNetworkReply::ProxyNotFoundError:
	case QNetworkReply::ProxyTimeoutError:
		return tr("Check proxy settings");
	case QNetworkReply::ProxyAuthenticationRequiredError:
		return tr("Check proxy username and password");
	default:
		return tr("Check internet connection");
	}
}
