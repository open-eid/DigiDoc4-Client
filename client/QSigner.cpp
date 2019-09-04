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
#include "widgets/CardWidget.h"
#include <common/QPCSC.h>
#include <common/TokenData.h>

#include <digidocpp/crypto/X509Cert.h>

#include <QtCore/QDebug>
#include <QtCore/QEventLoop>
#include <QtCore/QLoggingCategory>
#include <QtCore/QStringList>
#include <QtCore/QSet>
#include <QtNetwork/QSslKey>

#include <openssl/obj_mac.h>
#include <openssl/ecdsa.h>
#include <openssl/rsa.h>

Q_LOGGING_CATEGORY(SLog, "qdigidoc4.QSigner")

#if QT_VERSION < 0x050700
template <class T>
constexpr typename std::add_const<T>::type& qAsConst(T& t) noexcept
{
	return t;
}
#endif

#if OPENSSL_VERSION_NUMBER < 0x10100000L
static int ECDSA_SIG_set0(ECDSA_SIG *sig, BIGNUM *r, BIGNUM *s)
{
	if(!r || !s)
		return 0;
	BN_clear_free(sig->r);
	BN_clear_free(sig->s);
	sig->r = r;
	sig->s = s;
	return 1;
}
#endif


static bool isMatchingType(const SslCertificate &cert)
{
	// Check if cert is signing or authentication cert
	return cert.keyUsage().contains(SslCertificate::KeyEncipherment) ||
		   cert.keyUsage().contains(SslCertificate::KeyAgreement) ||
		   cert.keyUsage().contains(SslCertificate::NonRepudiation);
}

static QCardInfo *toCardInfo(const SslCertificate &c)
{
	QCardInfo *ci = new QCardInfo;
	ci->id = c.personalCode();
	if(ci->id.isEmpty())
		ci->id = c.serialNumber(true);
	ci->loading = false;
	ci->type = c.type();
	ci->c = c;
	return ci;
}


class QSigner::Private
{
public:
	QSigner::ApiType api = QSigner::PKCS11;
	QWin			*win = nullptr;
	QPKCS11			*pkcs11 = nullptr;
	QSmartCard		*smartcard = nullptr;
	TokenData		auth, sign;
	QMap<QString, QSharedPointer<QCardInfo>> cache;

	static QByteArray signData(int type, const QByteArray &digest, Private *d);
	static int rsa_sign(int type, const unsigned char *m, unsigned int m_len,
		unsigned char *sigret, unsigned int *siglen, const RSA *rsa);
	static ECDSA_SIG* ecdsa_do_sign(const unsigned char *dgst, int dgst_len,
		const BIGNUM *inv, const BIGNUM *rp, EC_KEY *eckey);

#if OPENSSL_VERSION_NUMBER < 0x10100000L || defined(LIBRESSL_VERSION_NUMBER)
	RSA_METHOD		rsamethod = *RSA_get_default_method();
	ECDSA_METHOD	*ecmethod = ECDSA_METHOD_new(nullptr);
#else
	RSA_METHOD		*rsamethod = RSA_meth_dup(RSA_get_default_method());
	EC_KEY_METHOD	*ecmethod = EC_KEY_METHOD_new(EC_KEY_get_default_method());
#endif
};

QByteArray QSigner::Private::signData(int type, const QByteArray &digest, Private *d)
{
#ifdef Q_OS_WIN
	if(d->win)
		return d->win->sign(type, digest);
#endif
	return d->pkcs11->sign(type, digest);
}

int QSigner::Private::rsa_sign(int type, const unsigned char *m, unsigned int m_len,
		unsigned char *sigret, unsigned int *siglen, const RSA *rsa)
{
	QByteArray result = signData(type, QByteArray::fromRawData((const char*)m, int(m_len)), (Private*)RSA_get_app_data(rsa));
	if(result.isEmpty())
		return 0;
	*siglen = (unsigned int)result.size();
	memcpy(sigret, result.constData(), size_t(result.size()));
	return 1;
}

ECDSA_SIG* QSigner::Private::ecdsa_do_sign(const unsigned char *dgst, int dgst_len,
		const BIGNUM * /*inv*/, const BIGNUM * /*rp*/, EC_KEY *eckey)
{
#if OPENSSL_VERSION_NUMBER < 0x10100000L
	Private *d = (Private*)ECDSA_get_ex_data(eckey, 0);
#else
	Private *d = (Private*)EC_KEY_get_ex_data(eckey, 0);
#endif
	QByteArray result = signData(0, QByteArray::fromRawData((const char*)dgst, dgst_len), d);
	if(result.isEmpty())
		return nullptr;
	QByteArray r = result.left(result.size()/2);
	QByteArray s = result.right(result.size()/2);
	ECDSA_SIG *sig = ECDSA_SIG_new();
	ECDSA_SIG_set0(sig,
		BN_bin2bn((const unsigned char*)r.data(), int(r.size()), nullptr),
		BN_bin2bn((const unsigned char*)s.data(), int(s.size()), nullptr));
	return sig;
}



using namespace digidoc;

QSigner::QSigner( ApiType api, QObject *parent )
	: QThread(parent)
	, d(new Private)
{
#if OPENSSL_VERSION_NUMBER < 0x10100000L || defined(LIBRESSL_VERSION_NUMBER)
	d->rsamethod.name = "QSmartCard";
	d->rsamethod.rsa_sign = Private::rsa_sign;
	d->rsamethod.flags |= RSA_FLAG_SIGN_VER;
	ECDSA_METHOD_set_name(d->ecmethod, const_cast<char*>("QSmartCard"));
	ECDSA_METHOD_set_sign(d->ecmethod, Private::ecdsa_do_sign);
	ECDSA_METHOD_set_app_data(d->ecmethod, d);
#else
	RSA_meth_set1_name(d->rsamethod, "QSmartCard");
	RSA_meth_set_sign(d->rsamethod, Private::rsa_sign);
	typedef int (*EC_KEY_sign)(int type, const unsigned char *dgst, int dlen, unsigned char *sig,
		unsigned int *siglen, const BIGNUM *kinv, const BIGNUM *r, EC_KEY *eckey);
	typedef int (*EC_KEY_sign_setup)(EC_KEY *eckey, BN_CTX *ctx_in, BIGNUM **kinvp, BIGNUM **rp);
	EC_KEY_sign sign = nullptr;
	EC_KEY_sign_setup sign_setup = nullptr;
	EC_KEY_METHOD_get_sign(d->ecmethod, &sign, &sign_setup, nullptr);
	EC_KEY_METHOD_set_sign(d->ecmethod, sign, sign_setup, Private::ecdsa_do_sign);
#endif

	d->api = api;
	d->auth.setCard(QStringLiteral("loading"));
	d->sign.setCard(QStringLiteral("loading"));
	d->smartcard = new QSmartCard(parent);
	connect(this, &QSigner::error, qApp, [](const QString &msg) {
		qApp->showWarning(msg);
	});
	start();
}

QSigner::~QSigner()
{
	requestInterruption();
	wait();
	delete d->smartcard;
#if OPENSSL_VERSION_NUMBER >= 0x10100000L
	RSA_meth_free(d->rsamethod);
	EC_KEY_METHOD_free(d->ecmethod);
#else
	ECDSA_METHOD_free(d->ecmethod);
#endif
	delete d;
}

const QMap<QString, QSharedPointer<QCardInfo>> QSigner::cache() const { return d->cache; }

void QSigner::cacheCardData(const QSet<QString> & /*cards*/)
{
	QList<TokenData> tokens;
#ifdef Q_OS_WIN
	if(d->win)
		tokens = d->win->tokens();
#endif
	if(d->pkcs11 && d->pkcs11->isLoaded())
		tokens = d->pkcs11->tokens();
	for(const TokenData &i: qAsConst(tokens))
	{
		if(!d->cache.contains(i.card()) || !d->cache[i.card()]->c.isValid())
		{
			auto sslCert = SslCertificate(i.cert());
			if(isMatchingType(sslCert))
				d->cache.insert(i.card(), QSharedPointer<QCardInfo>(toCardInfo(sslCert)));
		}
	}
}

X509Cert QSigner::cert() const
{
	if( d->sign.cert().isNull() )
		throw Exception(__FILE__, __LINE__, QSigner::tr("Sign certificate is not selected").toStdString());
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

QSslKey QSigner::key() const
{
	if(!QCardLock::instance().exclusiveTryLock())
		return QSslKey();

#ifdef Q_OS_WIN
	if(d->win)
		d->win->selectCert(d->auth.cert());
	else
#endif
	{
		switch(QPKCS11::PinStatus status = d->pkcs11->login(d->auth))
		{
		case QPKCS11::PinOK: break;
		case QPKCS11::PinIncorrect:
		case QPKCS11::PinLocked:
		default:
			QCardLock::instance().exclusiveUnlock();
			smartcard()->reload();
			return QSslKey();
		}
	}

	QSslKey key = d->auth.cert().publicKey();
	if(!key.handle())
	{
		QCardLock::instance().exclusiveUnlock();
		return key;
	}
	if (key.algorithm() == QSsl::Ec)
	{
		EC_KEY *ec = (EC_KEY*)key.handle();
#if OPENSSL_VERSION_NUMBER < 0x10100000L
		ECDSA_set_ex_data(ec, 0, d);
		ECDSA_set_method(ec, d->ecmethod);
#else
		EC_KEY_set_ex_data(ec, 0, d);
		EC_KEY_set_method(ec, d->ecmethod);
#endif
	}
	else
	{
		RSA *rsa = (RSA*)key.handle();
#if OPENSSL_VERSION_NUMBER < 0x10100000L || defined(LIBRESSL_VERSION_NUMBER)
		RSA_set_method(rsa, &d->rsamethod);
		rsa->flags |= RSA_FLAG_SIGN_VER;
#else
		RSA_set_method(rsa, d->rsamethod);
#endif
		RSA_set_app_data(rsa, d);
	}
	return key;
}

void QSigner::logout()
{
	if(d->pkcs11)
		d->pkcs11->logout();
	QCardLock::instance().exclusiveUnlock();
	d->smartcard->reload();
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
	d->auth.clear();
	d->auth.setCard(QStringLiteral("loading"));
	d->sign.clear();
	d->sign.setCard(QStringLiteral("loading"));

	switch( d->api )
	{
#ifdef Q_OS_WIN
	case CAPI: d->win = new QCSP( this ); break;
	case CNG: d->win = new QCNG( this ); break;
#endif
	default: d->pkcs11 = new QPKCS11(this); break;
	}

	while(!isInterruptionRequested())
	{
		if(QCardLock::instance().readTryLock())
		{
			if(d->pkcs11 && !d->pkcs11->reload())
			{
				Q_EMIT error(tr("Failed to load PKCS#11 module"));
				return;
			}

			TokenData aold = d->auth, at = aold;
			TokenData sold = d->sign, st = sold;
			QList<TokenData> tokens;
#ifdef Q_OS_WIN
			if(d->win)
				tokens = d->win->tokens();
#endif
			if(d->pkcs11)
				tokens = d->pkcs11->tokens();
			QStringList acards, scards;
			for(const TokenData &t: qAsConst(tokens))
			{
				SslCertificate c(t.cert());
				if(c.keyUsage().contains(SslCertificate::KeyEncipherment) ||
					c.keyUsage().contains(SslCertificate::KeyAgreement))
					acards << t.card();
				if(c.keyUsage().contains(SslCertificate::NonRepudiation))
					scards << t.card();
			}
			acards.removeDuplicates();
			scards.removeDuplicates();

			std::sort( acards.begin(), acards.end(), TokenData::cardsOrder );
			std::sort( scards.begin(), scards.end(), TokenData::cardsOrder );
			QStringList readers = QPCSC::instance().drivers();
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

			// if none is selected then pick first card with signing cert;
			// if no signing certs then pick first card with auth cert
			if(update && !scards.isEmpty())
				st.setCard(scards.first());
			if(update && !acards.isEmpty())
			{
				if(st.card().isEmpty())
					at.setCard(acards.first());
				else if(acards.contains(st.card()))
					at.setCard(st.card());
			}

			// read auth cert; if several cards with the same id exist (e.g. e-token
			// with expired and valid cert), then pick first valid cert with the id.
			if( acards.contains( at.card() ) && at.cert().isNull() )
			{
				for(const TokenData &i: qAsConst(tokens))
				{
					SslCertificate sslCert(i.cert());
					if(i.card() == at.card() &&
						(sslCert.keyUsage().contains(SslCertificate::KeyEncipherment) ||
						sslCert.keyUsage().contains(SslCertificate::KeyAgreement)))
					{
						at.setCert( i.cert() );
						at.setFlags( i.flags() );
						if(sslCert.isValid())
							break;
					}
				}
			}

			// read sign cert; if several cards with the same id exist (e.g. e-token
			// with expired and valid cert), then pick first valid cert with the id.
			if( scards.contains( st.card() ) && st.cert().isNull() )
			{
				qCDebug(SLog) << "Read sign cert" << st.card();
				for(const TokenData &i: qAsConst(tokens))
				{
					SslCertificate sslCert(i.cert());
					if( i.card() == st.card() && sslCert.keyUsage().contains( SslCertificate::NonRepudiation ) )
					{
						st.setCert( i.cert() );
						st.setFlags( i.flags() );
						if(sslCert.isValid())
							break;
					}
				}
				qCDebug(SLog) << "Cert is empty:" << st.cert().isNull();
			}

			auto added = scards.toSet().unite(acards.toSet()).subtract(d->cache.keys().toSet());
			if(!added.isEmpty())
				cacheCardData(added);

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
			d->smartcard->reloadCard(st.card().isEmpty() ? at.card() : st.card());

			QCardLock::instance().readUnlock();
		}

		if(!isInterruptionRequested())
			sleep( 5 );
	}
}

void QSigner::selectCard(const QString &card)
{
	TokenData t = d->auth;
	t.setCard( card );
	t.setCert( QSslCertificate() );
	Q_EMIT authDataChanged(d->auth = t);

	t = d->sign;
	t.setCard( card );
	t.setCert( QSslCertificate() );
	Q_EMIT signDataChanged(d->sign = t);
	Q_EMIT dataChanged();
}

std::vector<unsigned char> QSigner::sign(const std::string &method, const std::vector<unsigned char> &digest ) const
{
	#define throwException(msg, code) { \
		Exception e(__FILE__, __LINE__, (msg).toStdString()); \
		e.setCode(code); \
		throw e; \
	}

	if(!QCardLock::instance().exclusiveTryLock())
		throwException(tr("Signing/decrypting is already in progress another window."), Exception::General);

	if( !d->sign.cards().contains( d->sign.card() ) || d->sign.cert().isNull() )
	{
		QCardLock::instance().exclusiveUnlock();
		throwException(tr("Signing certificate is not selected."), Exception::General);
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
			throwException((tr("Failed to login token") + " " + QPKCS11::errorString(status)), Exception::PINCanceled);
		case QPKCS11::PinIncorrect:
			QCardLock::instance().exclusiveUnlock();
			reloadsign();
			throwException((tr("Failed to login token") + " " + QPKCS11::errorString(status)), Exception::PINIncorrect);
		case QPKCS11::PinLocked:
			QCardLock::instance().exclusiveUnlock();
			reloadsign();
			throwException((tr("Failed to login token") + " " + QPKCS11::errorString(status)), Exception::PINLocked);
		default:
			QCardLock::instance().exclusiveUnlock();
			throwException((tr("Failed to login token") + " " + QPKCS11::errorString(status)), Exception::General);
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
			throwException(tr("Failed to login token"), Exception::PINCanceled);
		}
	}
#endif

	QCardLock::instance().exclusiveUnlock();
	reloadsign();
	if( sig.isEmpty() )
		throwException(tr("Failed to sign document"), Exception::General);
	return std::vector<unsigned char>( sig.constBegin(), sig.constEnd() );
}

QSmartCard * QSigner::smartcard() const { return d->smartcard; }
TokenData QSigner::tokenauth() const { return d->auth; }
TokenData QSigner::tokensign() const { return d->sign; }
