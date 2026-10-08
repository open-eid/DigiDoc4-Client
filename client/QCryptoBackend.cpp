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
#include <QtCore/QMutex>
#include <QtCore/QReadWriteLock>
#include <QtCore/QWaitCondition>

static Q_LOGGING_CATEGORY(CryptoLog, "qdigidoc4.QCryptoManager")

struct QCryptoManager::Private
{
	QSmartCard smartcard;
	TokenData auth, sign;
	QList<TokenData> cache;
	QReadWriteLock operationLock;
	QReadWriteLock lock;
	QMutex sleepMutex;
	QWaitCondition sleepCond;
};

QCryptoBackend::~QCryptoBackend()
{
	auto *manager = qApp->cryptoManager();
	manager->smartcard()->reloadCard(token, true);
	manager->d->operationLock.unlock();
	manager->d->sleepCond.wakeAll();
}

std::expected<QCryptoBackend *,QCryptoBackend::Status>
QCryptoBackend::getBackend(const TokenData& token) {
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

QCryptoManager::QCryptoManager()
	: d(new Private)
{
	connect(&QPCSC::instance(), &QPCSC::cardChanged, this, [this] {
		qCDebug(CryptoLog) << "Card change detected";
		d->sleepCond.wakeAll();
	});
	start();
	QPCSC::instance().start();
}

QCryptoManager::~QCryptoManager()
{
	requestInterruption();
	d->sleepCond.wakeAll();
	wait();
	delete d;
}

void QCryptoManager::run()
{
	while(!isInterruptionRequested())
	{
		refresh();
		QMutexLocker locker(&d->sleepMutex);
		if(isInterruptionRequested())
			break;
		d->sleepCond.wait(&d->sleepMutex, 5000);
	}
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
	if(!isSign)
		Q_EMIT authDataChanged(token);
	else if(!other.isNull())
		Q_EMIT authDataChanged(other);
	d->smartcard.reloadCard(token, false);
}

void QCryptoManager::refresh()
{
	if(!d->operationLock.tryLockForRead())
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
	if(aold != anew)
		Q_EMIT authDataChanged(anew);
	if(sold != snew)
		d->smartcard.reloadCard(snew, false);
	else if(aold != anew)
		d->smartcard.reloadCard(anew, false);

	d->operationLock.unlock();
}
