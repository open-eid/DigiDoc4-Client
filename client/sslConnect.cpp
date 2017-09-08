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
 
#include "sslConnect_p.h"

#include <common/Common.h>
#include <common/Configuration.h>
#include <common/Settings.h>
#include <common/SOAPDocument.h>

#include <QtCore/QJsonObject>
#include <QtWidgets/QProgressBar>
#include <QtWidgets/QProgressDialog>

QByteArray HTTPRequest::request() const
{
	QByteArray r;
	r += m_method + " " + url().toEncoded( QUrl::RemoveScheme|QUrl::RemoveAuthority ) + " HTTP/" + m_ver + "\r\n";
	r += "Host: " + url().host() + "\r\n";
	r += "User-Agent: " + QString( "%1/%2 (%3)\r\n" )
		.arg( qApp->applicationName(), qApp->applicationVersion(), Common::applicationOs() ).toUtf8();
	foreach( const QByteArray &header, rawHeaderList() )
		r += header + ": " + rawHeader( header ) + "\r\n";

	if( !m_data.isEmpty() )
	{
		r += "Content-Length: " + QByteArray::number( m_data.size() ) + "\r\n\r\n";
		r += m_data;
	}
	else
		r += "\r\n";

	return r;
}



void SSLConnectPrivate::setError( const QString &msg )
{
	errorString = msg.isEmpty() ? ERR_reason_error_string( ERR_get_error() ) : msg;
}


void SSLConnectPrivate::run()
{
	char data[4096];
	int bytesRead = 0;
	while( (bytesRead = SSL_read( ssl, data, sizeof(data) )) > 0 )
		result.append( data, bytesRead );
	switch( SSL_get_error( ssl, bytesRead ) )
	{
	case SSL_ERROR_NONE:
	case SSL_ERROR_WANT_READ:
	case SSL_ERROR_WANT_WRITE:
	case SSL_ERROR_ZERO_RETURN: // Disconnect
	case SSL_ERROR_SYSCALL:
		break;
	default: setError();
	}
}



SSLConnect::SSLConnect( QObject *parent )
:	QObject( parent )
,	d( new SSLConnectPrivate() )
{
	d->ctx = SSL_CTX_new( TLSv1_client_method() );
	if( d->ctx )
		d->ssl = SSL_new( d->ctx );
	if( d->ssl )
		SSL_set_mode( d->ssl, SSL_MODE_AUTO_RETRY );
	else
		d->setError();
}

SSLConnect::~SSLConnect()
{
	if( d->ssl )
		SSL_shutdown( d->ssl );
	SSL_free( d->ssl );
	if( d->ctx )
		SSL_CTX_free( d->ctx );
	delete d;
}

QByteArray SSLConnect::getUrl( RequestType type, const QString &value )
{
	if( !d->ssl )
		return QByteArray();

	QJsonObject obj = Configuration::instance().object();
	QString label;
	QByteArray contentType;
	HTTPRequest req;
	switch( type )
	{
	case MobileInfo:
	{
		label = tr("Loading Mobile info");
		SOAPDocument s( "GetMIDTokens", "urn:GetMIDTokens" );
		s.writeEndDocument();
		req = HTTPRequest("POST", "1.1", obj.value("MID-URL").toString("https://id.sk.ee/MIDInfoWS/"));
		req.setRawHeader( "Content-Type", "text/xml" );
		req.setRawHeader( "SOAPAction", QByteArray() );
		req.setRawHeader( "Connection", "close" );
		req.setContent( s.document() );
		contentType = "text/xml";
		break;
	}
	case EmailInfo:
		label = tr("Loading Email info");
		req = HTTPRequest("GET", "1.0",
			obj.value("EMAIL-REDIRECT-URL").toString("https://sisene.www.eesti.ee/idportaal/postisysteem.naita_suunamised"));
		contentType = "application/xml";
		break;
	case ActivateEmails:
		label = tr("Loading Email info");
		req = HTTPRequest("GET", "1.0",
			obj.value("EMAIL-ACTIVATE-URL").toString("https://sisene.www.eesti.ee/idportaal/postisysteem.lisa_suunamine?=%1").arg(value));
		contentType = "application/xml";
		break;
	case PictureInfo:
		label = tr("Downloading picture");
		req = HTTPRequest("GET", "1.0",
			obj.value("PICTURE-URL").toString("https://sisene.www.eesti.ee/idportaal/portaal.idpilt"));
		contentType = "image/jpeg";
		break;
	default: return QByteArray();
	}

	QByteArray url = req.url().host().toUtf8();
	BIO *sock = BIO_new_connect( (char*)url.constData() );
	BIO_set_conn_port( sock, "https" );
	if( BIO_do_connect( sock ) <= 0 )
	{
		d->setError( tr( "Failed to connect to host. Are you connected to the internet?" ) );
		return QByteArray();
	}

	SSL_set_bio( d->ssl, sock, sock );
	if( !SSL_connect( d->ssl ) )
	{
		d->setError();
		return QByteArray();
	}

	QByteArray header = req.request();
	if( !SSL_write( d->ssl, header.constData(), header.size() ) )
	{
		d->setError();
		return QByteArray();
	}

	QProgressDialog p( label, QString(), 0, 0, qApp->activeWindow() );
	p.setWindowFlags( (p.windowFlags() | Qt::CustomizeWindowHint) & ~Qt::WindowCloseButtonHint );
	if( QProgressBar *bar = p.findChild<QProgressBar*>() )
		bar->setTextVisible( false );
	p.open();

	QEventLoop e;
	connect( d, SIGNAL(finished()), &e, SLOT(quit()) );
	d->start();
	e.exec();

	QMultiHash<QByteArray,QByteArray> headers;
	int pos = 0, old = 0;
	while((pos = d->result.indexOf("\r\n", old)) != -1)
	{
		if(pos == old)
			break;
		QByteArray header = d->result.mid(old, pos - old);
		if(headers.isEmpty())
			headers.insert("", header);
		else
		{
			int find = header.indexOf(": ");
			headers.insertMulti(header.left(find), header.mid(find + 2));
		}
		old = pos + 2;
	}

	if(!headers.value("").contains("200") ||
		!headers.value("Content-Type").contains(contentType))
	{
		d->setError( tr("Invalid response") );
		return QByteArray();
	}

	return d->result.mid( pos + 2 );
}

QString SSLConnect::errorString() const { return d->errorString; }

void SSLConnect::setToken( const QSslCertificate &cert, Qt::HANDLE key )
{
	d->cert = cert;
	if( !d->ssl )
		return d->setError( tr("SSL context is missing") );
	if( d->cert.isNull() )
		return d->setError( tr("Certificate is empty") );
	if( !SSL_use_certificate( d->ssl, (X509*)d->cert.handle() ) ||
		!SSL_use_PrivateKey( d->ssl, (EVP_PKEY*)key ) ||
		!SSL_check_private_key( d->ssl ) )
		d->setError();
}
