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
#include "QCNG.h"
#endif
#include "QPKCS11.h"
#include "SslCertificate.h"
#include "Utils.h"

#include <digidocpp/crypto/X509Cert.h>

#include <QtCore/QLoggingCategory>
#include <QtCore/QRegularExpression>
#include <QtNetwork/QSslKey>

#include <openssl/obj_mac.h>
#include <openssl/ecdsa.h>

#include <memory>

Q_LOGGING_CATEGORY(SLog, "qdigidoc4.QSigner")

class QSigner::Private final
{
public:
	QCryptoBackend	*backend = nullptr;
	QSmartCard		*smartcard = nullptr;
	TokenData		auth, sign;
	QList<TokenData> cache;

	static ECDSA_SIG* ecdsa_do_sign(const unsigned char *dgst, int dgst_len,
		const BIGNUM *inv, const BIGNUM *rp, EC_KEY *eckey);

	EC_KEY_METHOD	*ecmethod = EC_KEY_METHOD_new(EC_KEY_get_default_method());
};

ECDSA_SIG* QSigner::Private::ecdsa_do_sign(const unsigned char *dgst, int dgst_len,
		const BIGNUM * /*inv*/, const BIGNUM * /*rp*/, EC_KEY *eckey)
{
	QCryptoBackend *backend = (QCryptoBackend*)EC_KEY_get_ex_data(eckey, 0);
	QByteArray result = backend->sign(0, QByteArray::fromRawData((const char*)dgst, dgst_len));
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

QSigner::QSigner(QObject *parent)
	: QThread(parent)
	, d(new Private)
{
	using EC_KEY_sign = int (*)(int type, const unsigned char *dgst, int dlen, unsigned char *sig,
		unsigned int *siglen, const BIGNUM *kinv, const BIGNUM *r, EC_KEY *eckey);
	using EC_KEY_sign_setup = int (*)(EC_KEY *eckey, BN_CTX *ctx_in, BIGNUM **kinvp, BIGNUM **rp);
	EC_KEY_sign sign = nullptr;
	EC_KEY_sign_setup sign_setup = nullptr;
	EC_KEY_METHOD_get_sign(d->ecmethod, &sign, &sign_setup, nullptr);
	EC_KEY_METHOD_set_sign(d->ecmethod, sign, sign_setup, Private::ecdsa_do_sign);

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
	EC_KEY_METHOD_free(d->ecmethod);
	delete d;
}

QList<TokenData> QSigner::cache() const { return d->cache; }

QSet<QString> QSigner::cards() const
{
	QSet<QString> cards;
	for(const TokenData &t: qAsConst(d->cache))
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
	QRegularExpression reg(QStringLiteral("(\\w{1,2})(\\d{7})"));
	QRegularExpressionMatch r1 = reg.match(s1.card());
	QRegularExpressionMatch r2 = reg.match(s2.card());
	if(r1.hasMatch() || r2.hasMatch())
		return false;
	QStringList cap1 = r1.capturedTexts();
	QStringList cap2 = r1.capturedTexts();
	if(cap1.isEmpty() || cap2.isEmpty())
		return false;
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

QByteArray QSigner::decrypt(std::function<QByteArray (QCryptoBackend *)> &&func)
{
	if(!QCardLock::instance().exclusiveTryLock())
	{
		Q_EMIT error( tr("Signing/decrypting is already in progress another window.") );
		return {};
	}

	if( d->auth.cert().isNull() )
	{
		Q_EMIT error( tr("Authentication certificate is not selected.") );
		QCardLock::instance().exclusiveUnlock();
		return {};
	}

	QCryptoBackend::PinStatus status = QCryptoBackend::UnknownError;
	do
	{
		switch(status = d->backend->login(d->auth))
		{
		case QCryptoBackend::PinOK: break;
		case QCryptoBackend::PinCanceled:
			QCardLock::instance().exclusiveUnlock();
			return {};
		case QCryptoBackend::PinIncorrect:
			qApp->showWarning(QCryptoBackend::errorString(status));
			continue;
		case QCryptoBackend::PinLocked:
			QCardLock::instance().exclusiveUnlock();
			d->smartcard->reload(); // QSmartCard should also know that PIN1 is blocked.
			Q_EMIT error(QCryptoBackend::errorString(status));
			return {};
		default:
			QCardLock::instance().exclusiveUnlock();
			d->smartcard->reload(); // QSmartCard should also know that PIN1 is blocked.
			Q_EMIT error(tr("Failed to login token") + " " + QCryptoBackend::errorString(status));
			return {};
		}
	} while(status != QCryptoBackend::PinOK);
	QByteArray result;
	waitFor([&]{
		result = func(d->backend);
	});
	QCardLock::instance().exclusiveUnlock();
	d->backend->logout();
	d->smartcard->reload(); // QSmartCard should also know that PIN1 is blocked.
	if(d->backend->lastError() == QCryptoBackend::PinCanceled)
		return {};

	if(result.isEmpty())
		Q_EMIT error( tr("Failed to decrypt document") );
	return result;
}

QSslKey QSigner::key() const
{
	QSslKey key = d->auth.cert().publicKey();
	if(!key.handle() || key.algorithm() != QSsl::Ec)
		return {};
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

	EC_KEY *ec = (EC_KEY*)key.handle();
	EC_KEY_set_method(ec, d->ecmethod);
	EC_KEY_set_ex_data(ec, 0, d->backend);
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
#ifdef Q_OS_WIN
	d->backend = new QCNG(this);
#else
	d->backend = new QPKCS11(this);
#endif

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
		break;
	}
	d->smartcard->reloadCard(token);
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
	if(method == "http://www.w3.org/2001/04/xmldsig-more#rsa-sha224" ||
		method == "http://www.w3.org/2001/04/xmldsig-more#ecdsa-sha224") type = NID_sha224;
	if(method == "http://www.w3.org/2001/04/xmldsig-more#rsa-sha256" ||
		method == "http://www.w3.org/2001/04/xmldsig-more#ecdsa-sha256") type = NID_sha256;
	if(method == "http://www.w3.org/2001/04/xmldsig-more#rsa-sha384" ||
		method == "http://www.w3.org/2001/04/xmldsig-more#ecdsa-sha384") type = NID_sha384;
	if(method == "http://www.w3.org/2001/04/xmldsig-more#rsa-sha512" ||
		method == "http://www.w3.org/2001/04/xmldsig-more#ecdsa-sha512") type = NID_sha512;

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
	QByteArray sig;
	waitFor([&]{
		sig = d->backend->sign(type, QByteArray::fromRawData((const char*)digest.data(), int(digest.size())));
	});
	QCardLock::instance().exclusiveUnlock();
	d->backend->logout();
	d->smartcard->reload(); // QSmartCard should also know that PIN2 info is updated
	if(d->backend->lastError() == QCryptoBackend::PinCanceled)
		throwException(tr("Failed to login token"), Exception::PINCanceled)

	if( sig.isEmpty() )
		throwException(tr("Failed to sign document"), Exception::General)
	return {sig.constBegin(), sig.constEnd()};
}

QSmartCard * QSigner::smartcard() const { return d->smartcard; }
TokenData QSigner::tokenauth() const { return d->auth; }
TokenData QSigner::tokensign() const { return d->sign; }
