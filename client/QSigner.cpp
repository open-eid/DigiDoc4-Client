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

#include "Application.h"
#include "QCardLock.h"
#include "QSmartCard.h"
#include "TokenData.h"
#ifdef Q_OS_WIN
#include "QCSP.h"
#include "QCNG.h"
#endif
#include "QPKCS11.h"
#include "SslCertificate.h"

#include <digidocpp/crypto/X509Cert.h>

#include <QtCore/QLoggingCategory>
#include <QtNetwork/QSslKey>

#include <openssl/obj_mac.h>
#include <openssl/ecdsa.h>
#include <openssl/rsa.h>

Q_LOGGING_CATEGORY(SLog, "qdigidoc4.QSigner")

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

class QSigner::Private
{
public:
	QSigner::ApiType api = QSigner::PKCS11;
	QCryptoBackend	*backend = nullptr;
	QSmartCard		*smartcard = nullptr;
	TokenData		auth, sign;
	QList<TokenData> cache;

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
	return d->backend->sign(type, digest);
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
	using EC_KEY_sign = int (*)(int type, const unsigned char *dgst, int dlen, unsigned char *sig,
		unsigned int *siglen, const BIGNUM *kinv, const BIGNUM *r, EC_KEY *eckey);
	using EC_KEY_sign_setup = int (*)(EC_KEY *eckey, BN_CTX *ctx_in, BIGNUM **kinvp, BIGNUM **rp);
	EC_KEY_sign sign = nullptr;
	EC_KEY_sign_setup sign_setup = nullptr;
	EC_KEY_METHOD_get_sign(d->ecmethod, &sign, &sign_setup, nullptr);
	EC_KEY_METHOD_set_sign(d->ecmethod, sign, sign_setup, Private::ecdsa_do_sign);
#endif

	d->api = api;
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

QSigner::ApiType QSigner::apiType() const { return d->api; }

QList<TokenData> QSigner::cache() const { return d->cache; }

QSet<QString> QSigner::cards() const
{
	QSet<QString> cards;
	for(const TokenData &t: d->cache)
		cards.insert(t.card());
	return cards;
}

bool QSigner::cardsOrder(const TokenData &s1, const TokenData &s2)
{
	static auto cardsOrderScore = [](QChar c) -> quint8 {
		switch(c.toLatin1())
		{
		case 'N': return 6;
		case 'A': return 5;
		case 'P': return 4;
		case 'E': return 3;
		case 'F': return 2;
		case 'B': return 1;
		default: return 0;
		}
	};
	QRegExp r("(\\w{1,2})(\\d{7})");
	if(r.indexIn(s1.card()) == -1)
		return false;
	QStringList cap1 = r.capturedTexts();
	if(r.indexIn(s2.card()) == -1)
		return false;
	QStringList cap2 = r.capturedTexts();
	// new cards to front
	if(cap1[1].size() != cap2[1].size())
		return cap1[1].size() > cap2[1].size();
	// card type order
	if(cap1[1][0] != cap2[1][0])
		return cardsOrderScore(cap1[1][0]) > cardsOrderScore(cap2[1][0]);
	// card version order
	if(cap1[1].size() > 1 && cap2[1].size() > 1 && cap1[1][1] != cap2[1][1])
		return cap1[1][1] > cap2[1][1];
	// serial number order
	return cap1[2].toUInt() > cap2[2].toUInt();
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

	if( d->auth.cert().isNull() )
	{
		Q_EMIT error( tr("Authentication certificate is not selected.") );
		QCardLock::instance().exclusiveUnlock();
		return DecryptFailed;
	}

	QCryptoBackend::PinStatus status = QCryptoBackend::UnknownError;
	do
	{
		switch(status = d->backend->login(d->auth))
		{
		case QCryptoBackend::PinOK: break;
		case QCryptoBackend::PinCanceled:
			QCardLock::instance().exclusiveUnlock();
			return PinCanceled;
		case QCryptoBackend::PinIncorrect:
			qApp->showWarning(QCryptoBackend::errorString(status));
			continue;
		case QCryptoBackend::PinLocked:
			QCardLock::instance().exclusiveUnlock();
			d->smartcard->reload(); // QSmartCard should also know that PIN1 is blocked.
			Q_EMIT error(QCryptoBackend::errorString(status));
			return PinLocked;
		default:
			QCardLock::instance().exclusiveUnlock();
			d->smartcard->reload(); // QSmartCard should also know that PIN1 is blocked.
			Q_EMIT error(tr("Failed to login token") + " " + QCryptoBackend::errorString(status));
			return DecryptFailed;
		}
	} while(status != QCryptoBackend::PinOK);
	if(d->auth.cert().publicKey().algorithm() == QSsl::Rsa)
		out = d->backend->decrypt(in);
	else
		out = d->backend->deriveConcatKDF(in, digest, keySize, algorithmID, partyUInfo, partyVInfo);
	QCardLock::instance().exclusiveUnlock();
	d->backend->logout();
	d->smartcard->reload(); // QSmartCard should also know that PIN1 is blocked.
	if(d->backend->lastError() == QCryptoBackend::PinCanceled)
		return PinCanceled;

	if( out.isEmpty() )
		Q_EMIT error( tr("Failed to decrypt document") );
	return !out.isEmpty() ? DecryptOK : DecryptFailed;
}

QSslKey QSigner::key() const
{
	if(!QCardLock::instance().exclusiveTryLock())
		return {};

	QCryptoBackend::PinStatus status = QCryptoBackend::UnknownError;
	do
	{
		switch(status = d->backend->login(d->auth))
		{
		case QCryptoBackend::PinOK: break;
		case QCryptoBackend::PinIncorrect:
			qApp->showWarning(QCryptoBackend::errorString(status));
			continue;
		case QCryptoBackend::PinLocked:
		default:
			QCardLock::instance().exclusiveUnlock();
			d->smartcard->reload();
			return {};
		}
	} while(status != QCryptoBackend::PinOK);

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
	d->backend->logout();
	QCardLock::instance().exclusiveUnlock();
	d->smartcard->reload(); // QSmartCard should also know that PIN1 info is updated
}

void QSigner::run()
{
	d->auth.clear();
	d->sign.clear();

	switch( d->api )
	{
#ifdef Q_OS_WIN
	case CAPI: d->backend = new QCSP( this ); break;
	case CNG: d->backend = new QCNG( this ); break;
#endif
	default: d->backend = new QPKCS11(this); break;
	}

	while(!isInterruptionRequested())
	{
		if(QCardLock::instance().readTryLock())
		{
			QPKCS11 *pkcs11 = qobject_cast<QPKCS11*>(d->backend);
			if(pkcs11 && !pkcs11->reload())
			{
				Q_EMIT error(tr("Failed to load PKCS#11 module"));
				return;
			}

			TokenData aold = d->auth, at = aold;
			TokenData sold = d->sign, st = sold;
			QList<TokenData> acards, scards;
			QList<TokenData> cache = d->backend->tokens();
			std::sort(cache.begin(), cache.end(), cardsOrder);
			if(cache != d->cache)
			{
				d->cache = cache;
				Q_EMIT cacheChanged();
			}
			for(const TokenData &t: d->cache)
			{
				SslCertificate c(t.cert());
				if(c.keyUsage().contains(SslCertificate::KeyEncipherment) ||
					c.keyUsage().contains(SslCertificate::KeyAgreement))
					acards << t;
				if(c.keyUsage().contains(SslCertificate::NonRepudiation))
					scards << t;
			}

			// check if selected card is still in slot
			if(!at.isNull() && !acards.contains(at))
			{
				qCDebug(SLog) << "Disconnected from auth card" << st.card();
				at.clear();
			}
			if(!st.isNull() && !scards.contains(st))
			{
				qCDebug(SLog) << "Disconnected from sign card" << st.card();
				st.clear();
			}

			// if none is selected then pick first card with signing cert;
			// if no signing certs then pick first card with auth cert
			if(st.isNull() && !scards.isEmpty())
				st = scards.first();
			if(at.isNull() && !acards.isEmpty())
				at = acards.first();

			// update data if something has changed
			TokenData update;
			if(aold != at)
				Q_EMIT authDataChanged(d->auth = update = at);
			if(sold != st)
				Q_EMIT signDataChanged(d->sign = update = st);
			if(aold != at || sold != st)
				d->smartcard->reloadCard(update);
			QCardLock::instance().readUnlock();
		}

		if(!isInterruptionRequested())
			sleep( 5 );
	}
}

void QSigner::selectCard(const TokenData &token)
{
	bool isSign = SslCertificate(token.cert()).keyUsage().contains(SslCertificate::NonRepudiation);
	d->smartcard->reloadCard(token);
	if(isSign)
		Q_EMIT signDataChanged(d->sign = token);
	else
		Q_EMIT authDataChanged(d->auth = token);
	for(const TokenData &other: cache())
	{
		if(other == token || other.card() != token.card())
			continue;
		if(isSign) // Select other cert if they are on same card
			Q_EMIT authDataChanged(d->auth = other);
		else
			Q_EMIT signDataChanged(d->sign = other);
	}
}

std::vector<unsigned char> QSigner::sign(const std::string &method, const std::vector<unsigned char> &digest ) const
{
	#define throwException(msg, code) { \
		Exception e(__FILE__, __LINE__, (msg).toStdString()); \
		e.setCode(code); \
		throw e; \
	}

	if(!QCardLock::instance().exclusiveTryLock())
		throwException(tr("Signing/decrypting is already in progress another window."), Exception::General)

	if( d->sign.cert().isNull() )
	{
		QCardLock::instance().exclusiveUnlock();
		throwException(tr("Signing certificate is not selected."), Exception::General)
	}

	int type = NID_sha256;
	if( method == "http://www.w3.org/2001/04/xmldsig-more#rsa-sha224" ) type = NID_sha224;
	if( method == "http://www.w3.org/2001/04/xmldsig-more#rsa-sha256" ) type = NID_sha256;
	if( method == "http://www.w3.org/2001/04/xmldsig-more#rsa-sha384" ) type = NID_sha384;
	if( method == "http://www.w3.org/2001/04/xmldsig-more#rsa-sha512" ) type = NID_sha512;

	QCryptoBackend::PinStatus status = QCryptoBackend::UnknownError;
	do
	{
		switch(status = d->backend->login(d->sign))
		{
		case QCryptoBackend::PinOK: break;
		case QCryptoBackend::PinCanceled:
			QCardLock::instance().exclusiveUnlock();
			d->smartcard->reload(); // QSmartCard should also know that PIN2 info is updated
			throwException((tr("Failed to login token") + " " + QCryptoBackend::errorString(status)), Exception::PINCanceled)
		case QCryptoBackend::PinIncorrect:
			qApp->showWarning(QCryptoBackend::errorString(status));
			continue;
		case QCryptoBackend::PinLocked:
			QCardLock::instance().exclusiveUnlock();
			d->smartcard->reload(); // QSmartCard should also know that PIN2 info is updated
			throwException((tr("Failed to login token") + " " + QCryptoBackend::errorString(status)), Exception::PINLocked)
		default:
			QCardLock::instance().exclusiveUnlock();
			d->smartcard->reload(); // QSmartCard should also know that PIN2 info is updated
			throwException((tr("Failed to login token") + " " + QCryptoBackend::errorString(status)), Exception::PINFailed)
		}
	} while(status != QCryptoBackend::PinOK);
	QByteArray sig = d->backend->sign(type, QByteArray::fromRawData((const char*)digest.data(), int(digest.size())));
	QCardLock::instance().exclusiveUnlock();
	d->backend->logout();
	d->smartcard->reload(); // QSmartCard should also know that PIN2 info is updated
	if(d->backend->lastError() == QCryptoBackend::PinCanceled)
		throwException(tr("Failed to login token"), Exception::PINCanceled);

	if( sig.isEmpty() )
		throwException(tr("Failed to sign document"), Exception::General)
	return {sig.constBegin(), sig.constEnd()};
}

QSmartCard * QSigner::smartcard() const { return d->smartcard; }
TokenData QSigner::tokenauth() const { return d->auth; }
TokenData QSigner::tokensign() const { return d->sign; }
