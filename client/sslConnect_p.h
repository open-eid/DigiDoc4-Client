/*
 * QEstEidUtil
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

#pragma once

#include "sslConnect.h"

#include <QtCore/QThread>
#include <QtNetwork/QNetworkRequest>
#include <QtNetwork/QSslCertificate>

#include <openssl/err.h>
#include <openssl/ssl.h>

class HTTPRequest: public QNetworkRequest
{
public:
	HTTPRequest(): QNetworkRequest() {}
	HTTPRequest( const QByteArray &method, const QByteArray &ver, const QString &url )
	: QNetworkRequest( url ), m_method( method ), m_ver( ver ) {}

	void setContent( const QByteArray &data ) { m_data = data; }
	QByteArray request() const;

private:
	QByteArray m_data, m_method, m_ver;
};

class SSLConnectPrivate: public QThread
{
	Q_OBJECT
public:
	SSLConnectPrivate(): QThread(), ctx(0), ssl(0) {}

	void run();
	void setError( const QString &msg = QString() );

	SSL_CTX *ctx;
	SSL		*ssl;
	QString errorString;
	QByteArray result;
	QSslCertificate cert;
};
