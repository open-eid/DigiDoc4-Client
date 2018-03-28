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

#include "QSigner.h"
#include "QCardInfo.h"
#include "QCardLock.h"

#include "Application.h"

#ifdef Q_OS_WIN
#include "QCSP.h"
#include "QCNG.h"
#else
class QWin;
#endif
#include "QPKCS11.h"
#include <common/TokenData.h>

#include <digidocpp/crypto/X509Cert.h>

#include <QtCore/QDebug>
#include <QtCore/QEventLoop>
#include <QtCore/QLoggingCategory>
#include <QtCore/QStringList>
#include <QtCore/QSet>
#include <QtNetwork/QSslKey>

#include <openssl/obj_mac.h>

Q_LOGGING_CATEGORY(SLog, "qdigidoc4.QSigner")

#if QT_VERSION < 0x050700
template <class T>
constexpr typename std::add_const<T>::type& qAsConst(T& t) noexcept
{
	return t;
}
#endif

QCardInfo *toCardInfo(const SslCertificate &c)
{
	QCardInfo *ci = new QCardInfo;

	ci->country = c.toString("C");
	ci->id = c.subjectInfo("serialNumber");
	ci->isEResident = c.subjectInfo("O").contains("E-RESIDENT");
	ci->loading = false;
	ci->type = c.type();

	if(c.type() & SslCertificate::TempelType)
	{
		ci->fullName = c.toString("CN");
		ci->cardType = "e-Seal";
	}
	else
	{
		ci->fullName = c.toString("GN SN");
		ci->cardType = c.type() & SslCertificate::DigiIDType ? "Digi ID" : "ID Card";
	}

	return ci;
}

bool isMatchingType(const SslCertificate &cert, bool signing)
{
	// Check if cert is signing or authentication cert
	if(signing)
		return cert.keyUsage().contains(SslCertificate::NonRepudiation);
	
	return cert.keyUsage().contains(SslCertificate::KeyEncipherment) ||
		   cert.keyUsage().contains(SslCertificate::KeyAgreement);
}


class QSignerPrivate
{
public:
	QSigner::ApiType api = QSigner::PKCS11;
	QWin			*win = nullptr;
	QPKCS11Stack	*pkcs11 = nullptr;
	QSmartCard		*smartcard = nullptr;
	TokenData		auth, sign;
	volatile bool	terminate = false;
	QMap<QString, QSharedPointer<QCardInfo>> cache;
};

using namespace digidoc;

QSigner::QSigner( ApiType api, QObject *parent )
:	QThread( parent )
,	d( new QSignerPrivate )
{
	d->api = api;
	d->auth.setCard( "loading" );
	d->sign.setCard( "loading" );
	d->smartcard = new QSmartCard(parent);
	connect(this, &QSigner::error, this, &QSigner::showWarning);
	start();
}

QSigner::~QSigner()
{
	d->terminate = true;
	wait();
	delete d->smartcard;
	delete d;
}

const QMap<QString, QSharedPointer<QCardInfo>> QSigner::cache() const { return d->cache; }

void QSigner::cacheCardData(const QSet<QString> &cards, bool signingCert)
{
#ifdef Q_OS_WIN
	if(d->win)
	{
		QWin::Certs certs = d->win->certs();
		for(QWin::Certs::const_iterator i = certs.constBegin(); i != certs.constEnd(); ++i)
		{
			if(!d->cache.contains(i.value()) && isMatchingType(i.key(), signingCert))
				d->cache.insert(i.value(), QSharedPointer<QCardInfo>(toCardInfo(i.key())));
		}
	}
	else
#endif
	{
		QList<TokenData> pkcs11 = d->pkcs11->tokens();
		for(const TokenData &i: qAsConst(pkcs11))
		{
			if(!d->cache.contains(i.card()))
			{
				auto sslCert = SslCertificate(i.cert());
				if(isMatchingType(sslCert, signingCert))
					d->cache.insert(i.card(), QSharedPointer<QCardInfo>(toCardInfo(sslCert)));
			}
		}
	}
}

X509Cert QSigner::cert() const
{
	if( d->sign.cert().isNull() )
		throw Exception( __FILE__, __LINE__, QSigner::tr("Sign certificate is not selected").toUtf8().constData() );
	QByteArray der = d->sign.cert().toDer();
	return X509Cert((const unsigned char*)der.constData(), size_t(der.size()), X509Cert::Der);
}

QSigner::ErrorCode QSigner::decrypt(const QByteArray &in, QByteArray &out, const QString &digest, int keySize,
	const QByteArray &algorithmID, const QByteArray &partyUInfo, const QByteArray &partyVInfo)
{
	if(!QCardLock::instance().exclusiveTryLock())
	{
		Q_EMIT error( tr("Signing/decrypting is already in progress another window.") );
		return DecryptFailed;
	}

	if( !d->auth.cards().contains( d->auth.card() ) || d->auth.cert().isNull() )
	{
		Q_EMIT error( tr("Authentication certificate is not selected.") );
		QCardLock::instance().exclusiveUnlock();
		return DecryptFailed;
	}

	if( d->pkcs11 )
	{
		QPKCS11::PinStatus status = d->pkcs11->login( d->auth );
		switch( status )
		{
		case QPKCS11::PinOK: break;
		case QPKCS11::PinCanceled:
			QCardLock::instance().exclusiveUnlock();
			return PinCanceled;
		case QPKCS11::PinIncorrect:
			QCardLock::instance().exclusiveUnlock();
			reloadauth();
			if( !(d->auth.flags() & TokenData::PinLocked) )
			{
				Q_EMIT error( QPKCS11::errorString( status ) );
				return PinIncorrect;
			}
			// else pin locked, fall through
		case QPKCS11::PinLocked:
			QCardLock::instance().exclusiveUnlock();
			if (status != QPKCS11::PinIncorrect)
				reloadauth();
			Q_EMIT error( QPKCS11::errorString( status ) );
			return PinLocked;
		default:
			QCardLock::instance().exclusiveUnlock();
			Q_EMIT error( tr("Failed to login token") + " " + QPKCS11::errorString( status ) );
			return DecryptFailed;
		}
		if(d->auth.cert().publicKey().algorithm() == QSsl::Rsa)
			out = d->pkcs11->decrypt(in);
		else
			out = d->pkcs11->deriveConcatKDF(in, digest, keySize, algorithmID, partyUInfo, partyVInfo);
		d->pkcs11->logout();
	}
#ifdef Q_OS_WIN
	else if(d->win)
	{
		d->win->selectCert(d->auth.cert());
		if(d->auth.cert().publicKey().algorithm() == QSsl::Rsa)
			out = d->win->decrypt(in);
		else
			out = d->win->deriveConcatKDF(in, digest, keySize, algorithmID, partyUInfo, partyVInfo);
		if(d->win->lastError() == QWin::PinCanceled)
		{
			QCardLock::instance().exclusiveUnlock();
			smartcard()->reload(); // QSmartCard should also know that PIN1 is blocked.
			return PinCanceled;
		}
	}
#endif

	if( out.isEmpty() )
		Q_EMIT error( tr("Failed to decrypt document") );
	QCardLock::instance().exclusiveUnlock();
	reloadauth();
	return !out.isEmpty() ? DecryptOK : DecryptFailed;
}

void QSigner::reloadauth() const
{
	QEventLoop e;
	QObject::connect(this, &QSigner::authDataChanged, &e, &QEventLoop::quit);
	{
		QCardLocker locker;
		d->auth.setCert( QSslCertificate() );
	}
	e.exec();
}

void QSigner::reloadsign() const
{
	QEventLoop e;
	QObject::connect(this, &QSigner::signDataChanged, &e, &QEventLoop::quit);
	{
		QCardLocker locker;
		d->sign.setCert( QSslCertificate() );
	}
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
	case CAPI: d->win = new QCSP( this ); break;
	case CNG: d->win = new QCNG( this ); break;
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

		if(QCardLock::instance().readTryLock())
		{
			TokenData aold = d->auth, at = aold;
			TokenData sold = d->sign, st = sold;
			QStringList acards, scards, readers;
#ifdef Q_OS_WIN
			QWin::Certs certs;
			if(d->win)
			{
				certs = d->win->certs();
				for(QWin::Certs::const_iterator i = certs.constBegin(); i != certs.constEnd(); ++i)
				{
					if(i.key().keyUsage().contains(SslCertificate::KeyEncipherment) ||
						i.key().keyUsage().contains(SslCertificate::KeyAgreement))
						acards << i.value();
					if(i.key().keyUsage().contains(SslCertificate::NonRepudiation))
						scards << i.value();
				}
				readers << d->win->readers();
			}
#endif
			QList<TokenData> pkcs11;
			if( d->pkcs11 && d->pkcs11->isLoaded() )
			{
				pkcs11 = d->pkcs11->tokens();
				for(const TokenData &t: qAsConst(pkcs11))
				{
					SslCertificate c( t.cert() );
					if(c.keyUsage().contains(SslCertificate::KeyEncipherment) ||
						c.keyUsage().contains(SslCertificate::KeyAgreement))
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
				qCDebug(SLog) << "Disconnected from card" << st.card();
				st.setCard( QString() );
				st.setCert( QSslCertificate() );
			}

			// Some tokens (e.g. e-Seals) can have only authentication or signing cert;
			// Do not select first card in case if only one of the tokens is empty.
			bool update = at.card().isEmpty() && st.card().isEmpty();

			// if none is selected select first from cardlist
			if( update && !acards.isEmpty() )
				at.setCard( acards.first() );
			if( update && !scards.isEmpty() )
				st.setCard( scards.first() );

			if( acards.contains( at.card() ) && at.cert().isNull() ) // read auth cert
			{
#ifdef Q_OS_WIN
				if(d->win)
				{
					for(QWin::Certs::const_iterator i = certs.constBegin(); i != certs.constEnd(); ++i)
					{
						if(i.value() == at.card() &&
							(i.key().keyUsage().contains(SslCertificate::KeyEncipherment) ||
							i.key().keyUsage().contains(SslCertificate::KeyAgreement)))
						{
							at.setCert(i.key());
							break;
						}
					}
				}
				else
#endif
				{
					for(const TokenData &i: qAsConst(pkcs11))
					{
						if(i.card() == at.card() &&
							(SslCertificate(i.cert()).keyUsage().contains(SslCertificate::KeyEncipherment) ||
							SslCertificate(i.cert()).keyUsage().contains(SslCertificate::KeyAgreement)))
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
				qCDebug(SLog) << "Read sign cert" << st.card();
#ifdef Q_OS_WIN
				if(d->win)
				{
					for(QWin::Certs::const_iterator i = certs.constBegin(); i != certs.constEnd(); ++i)
					{
						qCDebug(SLog) << "Check card:" << i.value();
						if(i.value() == st.card() &&
							i.key().keyUsage().contains(SslCertificate::NonRepudiation))
						{
							st.setCert(i.key());
							break;
						}
					}
				}
				else
#endif
				{
					for(const TokenData &i: qAsConst(pkcs11))
					{
						if( i.card() == st.card() && SslCertificate( i.cert() ).keyUsage().contains( SslCertificate::NonRepudiation ) )
						{
							st.setCert( i.cert() );
							st.setFlags( i.flags() );
							break;
						}
					}
				}
				qCDebug(SLog) << "Cert is empty:" << st.cert().isNull();
			}

			auto added = scards.toSet().unite(acards.toSet()).subtract(d->cache.keys().toSet());
			if(!added.isEmpty())
				cacheCardData(added, st.card().isEmpty());

			bool changed = false;
			// update data if something has changed
			if(aold != at)
			{
				changed = true;
				Q_EMIT authDataChanged(d->auth = at);
			}
			if(sold != st)
			{
				changed = true;
				Q_EMIT signDataChanged(d->sign = st);
			}
			if(changed)
				Q_EMIT dataChanged();
			d->smartcard->reloadCard(st.card(), d->api != QSigner::CAPI);

			QCardLock::instance().readUnlock();
		}

		if(!d->terminate)
			sleep( 5 );
	}
}

void QSigner::selectAuthCard( const QString &card )
{
	TokenData t = d->auth;
	t.setCard( card );
	t.setCert( QSslCertificate() );
	Q_EMIT authDataChanged(d->auth = t);
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
	if(!QCardLock::instance().exclusiveTryLock())
		throwException( tr("Signing/decrypting is already in progress another window."), Exception::General, __LINE__ );

	if( !d->sign.cards().contains( d->sign.card() ) || d->sign.cert().isNull() )
	{
		QCardLock::instance().exclusiveUnlock();
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
			QCardLock::instance().exclusiveUnlock();
			throwException( tr("Failed to login token") + " " + QPKCS11::errorString( status ), Exception::PINCanceled, __LINE__ );
		case QPKCS11::PinIncorrect:
			QCardLock::instance().exclusiveUnlock();
			reloadsign();
			throwException( tr("Failed to login token") + " " + QPKCS11::errorString( status ), Exception::PINIncorrect, __LINE__ );
		case QPKCS11::PinLocked:
			QCardLock::instance().exclusiveUnlock();
			reloadsign();
			throwException( tr("Failed to login token") + " " + QPKCS11::errorString( status ), Exception::PINLocked, __LINE__ );
		default:
			QCardLock::instance().exclusiveUnlock();
			throwException( tr("Failed to login token") + " " + QPKCS11::errorString( status ), Exception::General, __LINE__ );
		}

		sig = d->pkcs11->sign(type, QByteArray::fromRawData((const char*)digest.data(), int(digest.size())));
		d->pkcs11->logout();
	}
#ifdef Q_OS_WIN
	else if(d->win)
	{
		d->win->selectCert(d->sign.cert());
		sig = d->win->sign(type, QByteArray::fromRawData((const char*)digest.data(), int(digest.size())));
		if(d->win->lastError() == QWin::PinCanceled)
		{
			QCardLock::instance().exclusiveUnlock();
			smartcard()->reload(); // QSmartCard should also know that PIN2 is blocked.
			throwException(tr("Failed to login token"), Exception::PINCanceled, __LINE__);
		}
	}
#endif

	QCardLock::instance().exclusiveUnlock();
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

QSmartCard * QSigner::smartcard() const { return d->smartcard; }
TokenData QSigner::tokenauth() const { return d->auth; }
TokenData QSigner::tokensign() const { return d->sign; }
