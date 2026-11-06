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
#else
#include "QPKCS11.h"
#endif
#include "QPCSC.h"
#include "SslCertificate.h"

#include <QtCore/QLoggingCategory>
#include <QtCore/QReadWriteLock>
#include <QtCore/QScopeGuard>
#include <QtCore/QTimer>

#include <algorithm>
#include <atomic>

struct QCryptoManager::Private
{
	QList<TokenData> cache;
	QReadWriteLock operationLock;
	QReadWriteLock lock;
	std::atomic<quint64> nextRequest {0};
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
	case InProgress: return tr("Signing/decrypting is already in progress in another window.");
	case GeneralError:
	case DeviceError:
		return tr("Error in smart card driver, reader or the card. On some readers it may indicate the wrong pin code or locked card.");
	default: return tr("Unknown error");
	}
}

QCryptoManager::QCryptoManager()
	: d(new Private)
{
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
		if(!std::ranges::equal(newCache, d->cache, [](const TokenData &a, const TokenData &b) {
			return a == b && a.data(QStringLiteral("blocked")) == b.data(QStringLiteral("blocked"));
		}))
		{
			d->cache = newCache;
			cacheChangedFlag = true;
		}
	}

	if(cacheChangedFlag)
		Q_EMIT cacheChanged(newCache);
}
