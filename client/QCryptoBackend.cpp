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

#include "QCryptoBackend.h"

#include "Application.h"
#ifdef Q_OS_WIN
#include "QCNG.h"
#endif
#include "QPCSC.h"
#include "QPKCS11.h"
#include "QSmartCard.h"
#include "SslCertificate.h"

#include <QtCore/QLoggingCategory>
#include <QtCore/QReadWriteLock>
#include <QtCore/QSemaphore>
#include <QtNetwork/QSslKey>

static Q_LOGGING_CATEGORY(CryptoLog, "qdigidoc4.QCryptoManager")

// TODO: Port everything to the new OpenSSL API
#define OPENSSL_SUPPRESS_DEPRECATED

#include <openssl/ecdsa.h>
#include <openssl/obj_mac.h>
#include <openssl/rsa.h>

struct QCryptoManager::Private
{
	QSmartCard smartcard;
	TokenData auth, sign;
	QList<TokenData> cache;
	QSemaphore operationLock {1};
	QReadWriteLock lock;

	RSA_METHOD *rsa_method = RSA_meth_dup(RSA_get_default_method());
	EC_KEY_METHOD *ec_method = EC_KEY_METHOD_new(EC_KEY_get_default_method());
};

QCryptoBackend::~QCryptoBackend()
{
	auto *manager = qApp->cryptoManager();
	// Reload counters while the operation semaphore is still held, otherwise
	// operationLock.release() would release it and queue refresh() first, letting the
	// manager thread enumerate tokens concurrently with this PCSC reload.
	manager->smartcard()->reloadCard(token, true);

	manager->d->operationLock.release();
	// A card change may have been skipped by refresh() while the operation held
	// the lock; re-sync on the main thread now that the card session is free.
	QMetaObject::invokeMethod(manager, [manager] { manager->refresh(); }, Qt::QueuedConnection);
}

std::expected<QCryptoBackend *,QCryptoBackend::Status>
QCryptoBackend::getBackend(const TokenData& token) {
	if(!qApp->cryptoManager()->d->operationLock.tryAcquire(1, (10 * 1000)))
		return std::unexpected(InProgress);
#ifdef Q_OS_WIN
	auto backend = std::make_unique<QCNG>();
#else
	auto backend = std::make_unique<QPKCS11>();
#endif
	backend->token = token;
	if(backend->cert().isNull())
		return backend.release();

	Status status;
	do {
		status = backend->login(token);
	} while (status == PinIncorrect);
	if (status != PinOK)
		return std::unexpected(status);

	return backend.release();
}

QSslCertificate QCryptoBackend::cert() const
{
	return token.cert();
}

QString QCryptoBackend::errorString(Status error)
{
	switch( error )
	{
	case PinOK: return QString();
	case PinCanceled: return tr("PIN entry canceled");
	case PinLocked: return tr("PIN locked");
	case PinIncorrect: return tr("PIN incorrect");
	case InProgress: return tr("Signing/decrypting is already in progress another window.");
	case GeneralError: return tr("PKCS11 general error");
	case DeviceError: return tr("PKCS11 device error");
	default: return tr("Unknown error");
	}
}

static int rsa_sign(int type, const unsigned char *m, unsigned int m_len, unsigned char *sigret, unsigned int *siglen, const RSA *rsa)
{
	auto *backend = (QCryptoBackend*) RSA_get_ex_data(rsa, 0);
	QCryptographicHash::Algorithm algo = QCryptographicHash::Sha256;
	switch(type)
	{
	case NID_sha224: algo = QCryptographicHash::Sha224; break;
	case NID_sha256: algo = QCryptographicHash::Sha256; break;
	case NID_sha384: algo = QCryptographicHash::Sha384; break;
	case NID_sha512: algo = QCryptographicHash::Sha512; break;
	}
	QByteArray result = backend->sign(algo, QByteArray::fromRawData((const char*)m, int(m_len)));
	if(result.isEmpty())
		return 0;
	*siglen = (unsigned int)result.size();
	memcpy(sigret, result.constData(), size_t(result.size()));
	return 1;
}

static ECDSA_SIG*
ecdsa_do_sign(const unsigned char *dgst, int dgst_len, const BIGNUM * /*inv*/, const BIGNUM * /*rp*/, EC_KEY *eckey)
{
	auto *backend = (QCryptoBackend*)EC_KEY_get_ex_data(eckey, 0);
	QByteArray result = backend->sign(QCryptographicHash::Sha512, QByteArray::fromRawData((const char*)dgst, dgst_len));
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

QSslKey
QCryptoBackend::getKey() const
{
	QSslKey key = token.cert().publicKey();
	if(!key.handle()) {
		status = GeneralError;
		return {};
	}
	auto *manager = qApp->cryptoManager();
	if(key.algorithm() == QSsl::Ec)
	{
		auto *ec = (EC_KEY*)key.handle();
		EC_KEY_set_method(ec, manager->d->ec_method);
		EC_KEY_set_ex_data(ec, 0, (void *) this);
	}
	else
	{
		RSA *rsa = (RSA*)key.handle();
		RSA_set_method(rsa, manager->d->rsa_method);
		RSA_set_ex_data(rsa, 0, (void *) this);
	}
	return key;
}

QCryptoManager::QCryptoManager()
	: d(new Private)
{
	RSA_meth_set1_name(d->rsa_method, "QSmartCard");
	RSA_meth_set_sign(d->rsa_method, rsa_sign);
	using EC_KEY_sign = int (*)(int type, const unsigned char *dgst, int dlen, unsigned char *sig,
		unsigned int *siglen, const BIGNUM *kinv, const BIGNUM *r, EC_KEY *eckey);
	using EC_KEY_sign_setup = int (*)(EC_KEY *eckey, BN_CTX *ctx_in, BIGNUM **kinvp, BIGNUM **rp);
	EC_KEY_sign sign = nullptr;
	EC_KEY_sign_setup sign_setup = nullptr;
	EC_KEY_METHOD_get_sign(d->ec_method, &sign, &sign_setup, nullptr);
	EC_KEY_METHOD_set_sign(d->ec_method, sign, sign_setup, ecdsa_do_sign);

	// Run the manager on its own thread with an event loop, so token enumeration
	// and reloadCard happen off the UI thread. cardChanged/selectCard are delivered
	// via queued connections and processed by run()'s exec() loop.
	moveToThread(this);
	connect(&QPCSC::instance(), &QPCSC::cardChanged, this, &QCryptoManager::refresh);
	QPCSC::instance().start();
	start();
}

QCryptoManager::~QCryptoManager()
{
	quit();
	wait();
	EC_KEY_METHOD_free(d->ec_method);
	RSA_meth_free(d->rsa_method);
	delete d;
}

void QCryptoManager::run()
{
	refresh();
	exec();
}

QList<TokenData> QCryptoManager::cache() const { QReadLocker locker(&d->lock); return d->cache; }
QSmartCard *QCryptoManager::smartcard() const { return &d->smartcard; }
TokenData QCryptoManager::tokenauth() const { QReadLocker locker(&d->lock); return d->auth; }
TokenData QCryptoManager::tokensign() const { QReadLocker locker(&d->lock); return d->sign; }

void QCryptoManager::selectCard(const TokenData &token)
{
	bool isSign = SslCertificate(token.cert()).keyUsage().contains(SslCertificate::NonRepudiation);
	TokenData other;
	{
		QWriteLocker locker(&d->lock);
		if(isSign)
			d->sign = token;
		else
			d->auth = token;
		for(const TokenData &t: d->cache)
		{
			if(t == token ||
				t.card() != token.card() ||
				isSign == SslCertificate(t.cert()).keyUsage().contains(SslCertificate::NonRepudiation))
				continue;
			if(isSign)
				d->auth = t;
			else
				d->sign = t;
			other = t;
			break;
		}
	}
	if(isSign)
		Q_EMIT signDataChanged(token);
	else
		Q_EMIT authDataChanged(token);
	if(!other.isNull())
	{
		if(isSign)
			Q_EMIT authDataChanged(other);
		else
			Q_EMIT signDataChanged(other);
	}
	d->smartcard.reloadCard(token, false);
}

void QCryptoManager::refresh()
{
	// Don't enumerate tokens while a sign/decrypt operation holds the card
	// session — the operation runs on a worker thread and PKCS11/PCSC access
	// from two threads at once is unsafe. unlockOperation() queues a catch-up
	// refresh when the operation finishes, so a skip here is not lost.
	if(!d->operationLock.tryAcquire())
		return;

#ifdef Q_OS_WIN
	QList<TokenData> newCache = QCNG::tokens();
#else
	QList<TokenData> newCache = QPKCS11::tokens();
#endif

	QList<TokenData> acards, scards;
	for(const TokenData &t: newCache)
	{
		SslCertificate c(t.cert());
		if(c.keyUsage().contains(SslCertificate::KeyEncipherment) ||
			c.keyUsage().contains(SslCertificate::KeyAgreement))
			acards.append(t);
		if(c.keyUsage().contains(SslCertificate::NonRepudiation))
			scards.append(t);
	}

	bool cacheChangedFlag = false;
	TokenData aold, sold, anew, snew;
	{
		QWriteLocker locker(&d->lock);
		if(newCache != d->cache)
		{
			d->cache = std::move(newCache);
			cacheChangedFlag = true;
		}

		aold = d->auth;
		sold = d->sign;

		if(!d->auth.isNull() && !acards.contains(d->auth))
		{
			qCDebug(CryptoLog) << "Disconnected from auth card" << d->auth.card();
			d->auth.clear();
		}
		if(!d->sign.isNull() && !scards.contains(d->sign))
		{
			qCDebug(CryptoLog) << "Disconnected from sign card" << d->sign.card();
			d->sign.clear();
		}

		if(d->sign.isNull() && !scards.isEmpty())
			d->sign = scards.first();
		if(d->auth.isNull() && !acards.isEmpty())
			d->auth = acards.first();

		anew = d->auth;
		snew = d->sign;
	}

	if(cacheChangedFlag)
		Q_EMIT cacheChanged();
	TokenData update;
	if(aold != anew)
		Q_EMIT authDataChanged(update = anew);
	if(sold != snew)
		Q_EMIT signDataChanged(update = snew);
	if(aold != anew || sold != snew)
		d->smartcard.reloadCard(update, false);

	d->operationLock.release();
}
