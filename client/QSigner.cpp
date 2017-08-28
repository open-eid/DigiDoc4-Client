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

#include "QSigner.h"

#include "Application.h"

#ifdef Q_OS_WIN
#include "QCSP.h"
#include "QCNG.h"
#else
class QCSP;
class QCNG;
#endif
#include "QPKCS11.h"
#include <common/TokenData.h>

#include <digidocpp/crypto/X509Cert.h>

#include <QtCore/QFile>
#include <QtCore/QEventLoop>
#include <QtCore/QStringList>
#include <QtCore/QSysInfo>
#include <QtNetwork/QSslKey>

#include <openssl/obj_mac.h>

class QSignerPrivate
{
public:
	QSigner::ApiType api = QSigner::PKCS11;
	QCSP			*csp = nullptr;
	QCNG			*cng = nullptr;
	QPKCS11Stack	*pkcs11 = nullptr;
	TokenData		auth, sign;
	volatile bool	terminate = false;
	QAtomicInt		count;
};

using namespace digidoc;

QSigner::QSigner( ApiType api, QObject *parent )
:	QThread( parent )
,	d( new QSignerPrivate )
{
	d->api = api;
	d->auth.setCard( "loading" );
	d->sign.setCard( "loading" );
	connect( this, SIGNAL(error(QString)), SLOT(showWarning(QString)) );
	start();
}

QSigner::~QSigner()
{
	d->terminate = true;
	wait();
	delete d;
}

X509Cert QSigner::cert() const
{
	if( d->sign.cert().isNull() )
		throw Exception( __FILE__, __LINE__, QSigner::tr("Sign certificate is not selected").toUtf8().constData() );
	QByteArray der = d->sign.cert().toDer();
	return X509Cert((const unsigned char*)der.constData(), size_t(der.size()), X509Cert::Der);
}

QSigner::ErrorCode QSigner::decrypt( const QByteArray &in, QByteArray &out )
{
	if( d->count.loadAcquire() > 0 )
	{
		Q_EMIT error( tr("Signing/decrypting is already in progress another window.") );
		return DecryptFailed;
	}

	d->count.ref();
	if( !d->auth.cards().contains( d->auth.card() ) || d->auth.cert().isNull() )
	{
		Q_EMIT error( tr("Authentication certificate is not selected.") );
		d->count.deref();
		return DecryptFailed;
	}

	if( d->pkcs11 )
	{
		QPKCS11::PinStatus status = d->pkcs11->login( d->auth );
		switch( status )
		{
		case QPKCS11::PinOK: break;
		case QPKCS11::PinCanceled:
			d->count.deref();
			return PinCanceled;
		case QPKCS11::PinIncorrect:
			d->count.deref();
			reloadauth();
			if( !(d->auth.flags() & TokenData::PinLocked) )
			{
				Q_EMIT error( QPKCS11::errorString( status ) );
				return PinIncorrect;
			}
			// else pin locked, fall through
		case QPKCS11::PinLocked:
			d->count.deref();
			Q_EMIT error( QPKCS11::errorString( status ) );
			return PinLocked;
		default:
			d->count.deref();
			Q_EMIT error( tr("Failed to login token") + " " + QPKCS11::errorString( status ) );
			return DecryptFailed;
		}
		out = d->pkcs11->decrypt( in );
		d->pkcs11->logout();
	}
#ifdef Q_OS_WIN
	else if( d->csp )
	{
		d->csp->selectCert( d->auth.cert() );
		out = d->csp->decrypt( in );
		if( d->csp->lastError() == QCSP::PinCanceled )
		{
			d->count.deref();
			return PinCanceled;
		}
	}
	else if( d->cng )
	{
		d->cng->selectCert( d->auth.cert() );
		out = d->cng->decrypt( in );
		if( d->cng->lastError() == QCNG::PinCanceled )
		{
			d->count.deref();
			return PinCanceled;
		}
	}
#endif

	if( out.isEmpty() )
		Q_EMIT error( tr("Failed to decrypt document") );
	d->count.deref();
	reloadauth();
	return !out.isEmpty() ? DecryptOK : DecryptFailed;
}

void QSigner::reloadauth() const
{
	QEventLoop e;
	QObject::connect( this, SIGNAL(authDataChanged(TokenData)), &e, SLOT(quit()) );
	d->count.ref();
	d->auth.setCert( QSslCertificate() );
	d->count.deref();
	e.exec();
}

void QSigner::reloadsign() const
{
	QEventLoop e;
	QObject::connect( this, SIGNAL(signDataChanged(TokenData)), &e, SLOT(quit()) );
	d->count.ref();
	d->sign.setCert( QSslCertificate() );
	d->count.deref();
	e.exec();
}

void QSigner::run()
{
	d->terminate = false;
	d->auth.clear();
	d->auth.setCard( "loading" );
	d->sign.clear();
	d->sign.setCard( "loading" );

	switch( d->api )
	{
#ifdef Q_OS_WIN
	case CAPI: d->csp = new QCSP( this ); break;
	case CNG: d->cng = new QCNG( this ); break;
#endif
	default: d->pkcs11 = new QPKCS11Stack( this ); break;
	}

	QString driver = qApp->confValue( Application::PKCS11Module ).toString();
	while( !d->terminate )
	{
		if( d->pkcs11 && !d->pkcs11->isLoaded() && !d->pkcs11->load( driver ) )
		{
			Q_EMIT error( tr("Failed to load PKCS#11 module") + "\n" + driver );
			return;
		}

		if( !d->count.loadAcquire() )
		{
			d->count.deref();
			TokenData aold = d->auth, at = aold;
			TokenData sold = d->sign, st = sold;
			QStringList acards, scards, readers;
#ifdef Q_OS_WIN
			QCNG::Certs certs;
			if( d->csp )
			{
				certs = d->csp->certs();
				for( QCNG::Certs::const_iterator i = certs.constBegin(); i != certs.constEnd(); ++i )
				{
					if( i.key().keyUsage().contains( SslCertificate::KeyEncipherment ) )
						acards << i.value();
					if( i.key().keyUsage().contains( SslCertificate::NonRepudiation ) )
						scards << i.value();
				}
				readers << "blank";
			}
			if( d->cng )
			{
				certs = d->cng->certs();
				for( QCNG::Certs::const_iterator i = certs.constBegin(); i != certs.constEnd(); ++i )
				{
					if( i.key().keyUsage().contains( SslCertificate::KeyEncipherment ) )
						acards << i.value();
					if( i.key().keyUsage().contains( SslCertificate::NonRepudiation ) )
						scards << i.value();
				}
				readers << d->cng->readers();
			}
#endif
			QList<TokenData> pkcs11;
			if( d->pkcs11 && d->pkcs11->isLoaded() )
			{
				pkcs11 = d->pkcs11->tokens();
				Q_FOREACH( const TokenData &t, pkcs11 )
				{
					SslCertificate c( t.cert() );
					if( c.keyUsage().contains( SslCertificate::KeyEncipherment ) )
						acards << t.card();
					if( c.keyUsage().contains( SslCertificate::NonRepudiation ) )
						scards << t.card();
				}
				acards.removeDuplicates();
				scards.removeDuplicates();
				readers = d->pkcs11->readers();
			}

			std::sort( acards.begin(), acards.end(), TokenData::cardsOrder );
			std::sort( scards.begin(), scards.end(), TokenData::cardsOrder );
			std::sort( readers.begin(), readers.end() );
			at.setCards( acards );
			at.setReaders( readers );
			st.setCards( scards );
			st.setReaders( readers );

			// check if selected card is still in slot
			if( !at.card().isEmpty() && !acards.contains( at.card() ) )
			{
				at.setCard( QString() );
				at.setCert( QSslCertificate() );
			}
			if( !st.card().isEmpty() && !scards.contains( st.card() ) )
			{
				st.setCard( QString() );
				st.setCert( QSslCertificate() );
			}

			// if none is selected select first from cardlist
			if( at.card().isEmpty() && !acards.isEmpty() )
				at.setCard( acards.first() );
			if( st.card().isEmpty() && !scards.isEmpty() )
				st.setCard( scards.first() );

			if( acards.contains( at.card() ) && at.cert().isNull() ) // read auth cert
			{
#ifdef Q_OS_WIN
				if( d->csp || d->cng )
				{
					for( QCNG::Certs::const_iterator i = certs.constBegin(); i != certs.constEnd(); ++i )
					{
						if( i.value() == at.card() &&
							i.key().keyUsage().contains( SslCertificate::KeyEncipherment ) )
						{
							at.setCert( i.key() );
							break;
						}
					}
				}
				else
#endif
				{
					Q_FOREACH( const TokenData &i, pkcs11 )
					{
						if( i.card() == at.card() && SslCertificate( i.cert() ).keyUsage().contains( SslCertificate::KeyEncipherment ) )
						{
							at.setCert( i.cert() );
							at.setFlags( i.flags() );
							break;
						}
					}
				}
			}

			if( scards.contains( st.card() ) && st.cert().isNull() ) // read sign cert
			{
#ifdef Q_OS_WIN
				if( d->csp || d->cng )
				{
					for( QCNG::Certs::const_iterator i = certs.constBegin(); i != certs.constEnd(); ++i )
					{
						if( i.value() == st.card() &&
							i.key().keyUsage().contains( SslCertificate::NonRepudiation ) )
						{
							st.setCert( i.key() );
							break;
						}
					}
				}
				else
#endif
				{
					Q_FOREACH( const TokenData &i, pkcs11 )
					{
						if( i.card() == st.card() && SslCertificate( i.cert() ).keyUsage().contains( SslCertificate::NonRepudiation ) )
						{
							st.setCert( i.cert() );
							st.setFlags( i.flags() );
							break;
						}
					}
				}
			}

			// update data if something has changed
			if( aold != at )
				Q_EMIT authDataChanged(d->auth = at);
			if( sold != st )
				Q_EMIT signDataChanged(d->sign = st);
			d->count.ref();
		}

		sleep( 5 );
	}
}

void QSigner::selectAuthCard( const QString &card )
{
	TokenData t = d->auth;
	t.setCard( card );
	t.setCert( QSslCertificate() );
	Q_EMIT signDataChanged(d->auth = t);
}

void QSigner::selectSignCard( const QString &card )
{
	TokenData t = d->sign;
	t.setCard( card );
	t.setCert( QSslCertificate() );
	Q_EMIT signDataChanged(d->sign = t);
}

void QSigner::showWarning( const QString &msg )
{ qApp->showWarning( msg ); }

std::vector<unsigned char> QSigner::sign(const std::string &method, const std::vector<unsigned char> &digest ) const
{
	if( d->count.loadAcquire() > 0 )
		throwException( tr("Signing/decrypting is already in progress another window."), Exception::General, __LINE__ );

	d->count.ref();
	if( !d->sign.cards().contains( d->sign.card() ) || d->sign.cert().isNull() )
	{
		d->count.deref();
		throwException( tr("Signing certificate is not selected."), Exception::General, __LINE__ );
	}

	int type = NID_sha256;
	if( method == "http://www.w3.org/2001/04/xmldsig-more#rsa-sha224" ) type = NID_sha224;
	if( method == "http://www.w3.org/2001/04/xmldsig-more#rsa-sha256" ) type = NID_sha256;
	if( method == "http://www.w3.org/2001/04/xmldsig-more#rsa-sha384" ) type = NID_sha384;
	if( method == "http://www.w3.org/2001/04/xmldsig-more#rsa-sha512" ) type = NID_sha512;

	QByteArray sig;
	if( d->pkcs11 )
	{
		QPKCS11::PinStatus status = d->pkcs11->login( d->sign );
		switch( status )
		{
		case QPKCS11::PinOK: break;
		case QPKCS11::PinCanceled:
			d->count.deref();
			throwException( tr("Failed to login token") + " " + QPKCS11::errorString( status ), Exception::PINCanceled, __LINE__ );
		case QPKCS11::PinIncorrect:
			d->count.deref();
			throwException( tr("Failed to login token") + " " + QPKCS11::errorString( status ), Exception::PINIncorrect, __LINE__ );
		case QPKCS11::PinLocked:
			d->count.deref();
			reloadsign();
			throwException( tr("Failed to login token") + " " + QPKCS11::errorString( status ), Exception::PINLocked, __LINE__ );
		default:
			d->count.deref();
			throwException( tr("Failed to login token") + " " + QPKCS11::errorString( status ), Exception::General, __LINE__ );
		}

		sig = d->pkcs11->sign( type, QByteArray::fromRawData( (const char*)&digest[0], int(digest.size()) ) );
		d->pkcs11->logout();
	}
#ifdef Q_OS_WIN
	else if( d->csp )
	{
		d->csp->selectCert( d->sign.cert() );
		sig = d->csp->sign( type, QByteArray::fromRawData( (const char*)&digest[0], int(digest.size()) ) );
		if( d->csp->lastError() == QCSP::PinCanceled )
		{
			d->count.deref();
			throwException( tr("Failed to login token"), Exception::PINCanceled, __LINE__ );
		}
	}
	else if( d->cng )
	{
		d->cng->selectCert( d->sign.cert() );
		sig = d->cng->sign( type, QByteArray::fromRawData( (const char*)&digest[0], int(digest.size()) ) );
		if( d->cng->lastError() == QCNG::PinCanceled )
		{
			d->count.deref();
			throwException( tr("Failed to login token"), Exception::PINCanceled, __LINE__ );
		}
	}
#endif

	d->count.deref();
	reloadsign();
	if( sig.isEmpty() )
		throwException( tr("Failed to sign document"), Exception::General, __LINE__ );
	return std::vector<unsigned char>( sig.constBegin(), sig.constEnd() );
}

void QSigner::throwException( const QString &msg, Exception::ExceptionCode code, int line ) const
{
	QString t = msg;
	Exception e( __FILE__, line, t.toUtf8().constData() );
	e.setCode( code );
	throw e;
}

TokenData QSigner::tokenauth() const { return d->auth; }
TokenData QSigner::tokensign() const { return d->sign; }
