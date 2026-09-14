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
#include "SslCertificate.h"

#include <QtCore/QLoggingCategory>
#include <QtCore/QReadWriteLock>
#include <QtCore/QScopeGuard>
#include <QtCore/QTimer>
#include <QtNetwork/QSslKey>

// TODO: Port everything to the new OpenSSL API
#define OPENSSL_SUPPRESS_DEPRECATED

#include <openssl/ecdsa.h>
#include <openssl/obj_mac.h>
#include <openssl/rsa.h>

#include <atomic>

struct QCryptoManager::Private
{
	QList<TokenData> cache;
	QReadWriteLock operationLock;
	QReadWriteLock lock;
	std::atomic<quint64> nextRequest {0};

	RSA_METHOD *rsa_method = RSA_meth_dup(RSA_get_default_method());
	EC_KEY_METHOD *ec_method = EC_KEY_METHOD_new(EC_KEY_get_default_method());
};

QCryptoBackend::~QCryptoBackend()
{
	auto *manager = qApp->cryptoManager();
	manager->d->operationLock.unlock();
	manager->refreshToken(token);
}

std::expected<QCryptoBackend *,QCryptoBackend::Status>
QCryptoBackend::getBackend(const TokenData& token, const QString &pin) {
	if(!qApp->cryptoManager()->d->operationLock.tryLockForWrite(10 * 1000))
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
	if(pin.isEmpty())
	{
		do {
			status = backend->login(token, {});
		} while (status == PinIncorrect);
	}
	else
		status = backend->login(token, pin);
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

	// Token enumeration runs on the manager thread.
	moveToThread(this);
	connect(&QPCSC::instance(), &QPCSC::cardChanged, this, &QCryptoManager::refresh);
	start();
	QPCSC::instance().start();
}

QCryptoManager::~QCryptoManager()
{
	requestInterruption();
	quit();
	wait();
	EC_KEY_METHOD_free(d->ec_method);
	RSA_meth_free(d->rsa_method);
	delete d;
}

void QCryptoManager::run()
{
	QTimer timer;
	connect(&timer, &QTimer::timeout, this, &QCryptoManager::refresh);
	timer.start(5000);
	refresh();
	exec();
}

QList<TokenData> QCryptoManager::cache() const { QReadLocker locker(&d->lock); return d->cache; }

quint64 QCryptoManager::readCard(const TokenData &token, const QSmartCardData &previous)
{
	const quint64 request = ++d->nextRequest;
	QMetaObject::invokeMethod(this, [this, token, previous, request] {
		QSmartCardData data;
		if(isInterruptionRequested())
			return;
		if(cache().contains(token))
			data = previous.isNull() ? QSmartCard::readCard(token) : QSmartCard::readCounters(previous);
		Q_EMIT cardDataReady(request, data);
	}, Qt::QueuedConnection);
	return request;
}

void QCryptoManager::refreshToken(const TokenData &token)
{
	QMetaObject::invokeMethod(this, [this, token] {
		refresh();
		if(!token.isNull())
			Q_EMIT tokenChanged(token);
	}, Qt::QueuedConnection);
}

void QCryptoManager::refresh()
{
	if(!d->operationLock.tryLockForRead())
		return;
	auto unlock = qScopeGuard([this] {
		d->operationLock.unlock();
		Q_EMIT refreshed();
	});

#ifdef Q_OS_WIN
	QList<TokenData> newCache = QCNG::tokens();
#else
	QList<TokenData> newCache = QPKCS11::tokens();
#endif

	bool cacheChangedFlag = false;
	{
		QWriteLocker locker(&d->lock);
		if(newCache != d->cache)
		{
			d->cache = newCache;
			cacheChangedFlag = true;
		}
	}

	if(cacheChangedFlag)
		Q_EMIT cacheChanged(newCache);
}
