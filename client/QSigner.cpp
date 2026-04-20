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
#include "QPCSC.h"
#include "QSmartCard.h"
#include "TokenData.h"
#ifdef Q_OS_WIN
#include "QCNG.h"
#endif
#include "QPKCS11.h"
#include "SslCertificate.h"
#include "Utils.h"
#include "dialogs/WarningDialog.h"

#include <digidocpp/Conf.h>
#include <digidocpp/crypto/X509Cert.h>

#include <QtCore/QLoggingCategory>
#include <QtCore/QMutex>
#include <QtCore/QReadWriteLock>
#include <QtCore/QWaitCondition>
#include <QtNetwork/QSslKey>

#include <openssl/ecdsa.h>
#include <openssl/obj_mac.h>
#include <openssl/rsa.h>

static Q_LOGGING_CATEGORY(SLog, "qdigidoc4.QSigner")

class QSigner::Private final
{
public:
	QSmartCard		*smartcard {};
	TokenData		auth, sign;
	QList<TokenData> cache;
	QReadWriteLock	lock;
	QMutex			sleepMutex;
	QWaitCondition	sleepCond;

	//static ECDSA_SIG* ecdsa_do_sign(const unsigned char *dgst, int dgst_len,
	//	const BIGNUM *inv, const BIGNUM *rp, EC_KEY *eckey);
	//static int rsa_sign(int type, const unsigned char *m, unsigned int m_len,
	//	unsigned char *sigret, unsigned int *siglen, const RSA *rsa);

	//RSA_METHOD		*rsamethod = RSA_meth_dup(RSA_get_default_method());
	//EC_KEY_METHOD	*ecmethod = EC_KEY_METHOD_new(EC_KEY_get_default_method());
};



using namespace digidoc;

QSigner::QSigner(QObject *parent)
	: QThread(parent)
	, d(new Private)
{
	d->smartcard = new QSmartCard(parent);
	connect(this, &QSigner::error, this, [](const QString &title, const QString &msg) {
		WarningDialog::create()
			->withTitle(title)
			->withText(msg)
			->open();
	});
	connect(this, &QSigner::signDataChanged, this, [this](const TokenData &token) {
		std::string method;
		if(token.data(QStringLiteral("PSS")).toBool())
		{
			switch(methodToNID(CONF(signatureDigestUri)))
			{
			case QCryptographicHash::Sha224: method = "http://www.w3.org/2007/05/xmldsig-more#sha224-rsa-MGF1"; break;
			case QCryptographicHash::Sha256: method = "http://www.w3.org/2007/05/xmldsig-more#sha256-rsa-MGF1"; break;
			case QCryptographicHash::Sha384: method = "http://www.w3.org/2007/05/xmldsig-more#sha384-rsa-MGF1"; break;
			case QCryptographicHash::Sha512: method = "http://www.w3.org/2007/05/xmldsig-more#sha512-rsa-MGF1"; break;
			default: break;
			}
		}
		setMethod(method);
	});
	connect(&QPCSC::instance(), &QPCSC::cardChanged, this, [this] {
		qCDebug(SLog) << "Card change detected";
		d->sleepCond.wakeAll();
	});
	start();
	QPCSC::instance().start();
}

QSigner::~QSigner()
{
	requestInterruption();
	d->sleepCond.wakeAll();
	wait();
	delete d->smartcard;
	delete d;
}

QList<TokenData> QSigner::cache() const { return d->cache; }

X509Cert QSigner::cert() const
{
	if( d->sign.cert().isNull() )
		throw Exception(__FILE__, __LINE__, QSigner::tr("Sign certificate is not selected").toStdString());
	QByteArray der = d->sign.cert().toDer();
	return X509Cert((const unsigned char*)der.constData(), size_t(der.size()), X509Cert::Der);
}

void QSigner::logout() const
{
	d->lock.unlock();
	// QSmartCard should also know that PIN1 info is updated
	d->smartcard->reloadCard(d->smartcard->tokenData(), true);
}

QCryptographicHash::Algorithm QSigner::methodToNID(const std::string &method)
{
	if(method == "http://www.w3.org/2001/04/xmldsig-more#sha224" ||
		method == "http://www.w3.org/2001/04/xmldsig-more#rsa-sha224" ||
		method == "http://www.w3.org/2007/05/xmldsig-more#sha224-rsa-MGF1" ||
		method == "http://www.w3.org/2001/04/xmldsig-more#ecdsa-sha224") return QCryptographicHash::Sha224;
	if(method == "http://www.w3.org/2001/04/xmlenc#sha256" ||
		method == "http://www.w3.org/2001/04/xmldsig-more#rsa-sha256" ||
		method == "http://www.w3.org/2007/05/xmldsig-more#sha256-rsa-MGF1" ||
		method == "http://www.w3.org/2001/04/xmldsig-more#ecdsa-sha256") return QCryptographicHash::Sha256;
	if(method == "http://www.w3.org/2001/04/xmldsig-more#sha384" ||
		method == "http://www.w3.org/2001/04/xmldsig-more#rsa-sha384" ||
		method == "http://www.w3.org/2007/05/xmldsig-more#sha384-rsa-MGF1" ||
		method == "http://www.w3.org/2001/04/xmldsig-more#ecdsa-sha384") return QCryptographicHash::Sha384;
	if(method == "http://www.w3.org/2001/04/xmlenc#sha512" ||
		method == "http://www.w3.org/2001/04/xmldsig-more#rsa-sha512" ||
		method == "http://www.w3.org/2007/05/xmldsig-more#sha512-rsa-MGF1"  ||
		method == "http://www.w3.org/2001/04/xmldsig-more#ecdsa-sha512") return QCryptographicHash::Sha512;
	return QCryptographicHash::Sha256;
}

void QSigner::run()
{
	d->auth.clear();
	d->sign.clear();

	while(!isInterruptionRequested()) {
		if(d->lock.tryLockForRead()) {
			QList<TokenData> acards, scards;
			QList<TokenData> cache = QCryptoBackend::getTokens();
			if(cache != d->cache)
			{
				d->cache = std::move(cache);
				Q_EMIT cacheChanged();
			}
			for(const TokenData &t: d->cache)
			{
				SslCertificate c(t.cert());
				if(c.keyUsage().contains(SslCertificate::KeyEncipherment) ||
					c.keyUsage().contains(SslCertificate::KeyAgreement))
					acards.append(t);
				if(c.keyUsage().contains(SslCertificate::NonRepudiation))
					scards.append(t);
			}

			TokenData aold = d->auth;
			TokenData sold = d->sign;
			// check if selected card is still in slot
			if(!d->auth.isNull() && !acards.contains(d->auth))
			{
				qCDebug(SLog) << "Disconnected from auth card" << d->auth.card();
				d->auth.clear();
			}
			if(!d->sign.isNull() && !scards.contains(d->sign))
			{
				qCDebug(SLog) << "Disconnected from sign card" << d->sign.card();
				d->sign.clear();
			}

			// if none is selected then pick first card with signing cert;
			// if no signing certs then pick first card with auth cert
			if(d->sign.isNull() && !scards.isEmpty())
				d->sign = scards.first();
			if(d->auth.isNull() && !acards.isEmpty())
				d->auth = acards.first();

			// update data if something has changed
			TokenData update;
			if(aold != d->auth)
				Q_EMIT authDataChanged(update = d->auth);
			if(sold != d->sign)
				Q_EMIT signDataChanged(update = d->sign);
			if(aold != d->auth || sold != d->sign)
				d->smartcard->reloadCard(update, false);
			d->lock.unlock();
		}

		QMutexLocker locker(&d->sleepMutex);
		if (isInterruptionRequested())
			break;
		d->sleepCond.wait(&d->sleepMutex, 5000);
	}
	QCryptoBackend::shutDown();
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
		if(other == token ||
			other.card() != token.card() ||
			isSign == SslCertificate(other.cert()).keyUsage().contains(SslCertificate::NonRepudiation))
			continue;
		if(isSign) // Select other cert if they are on same card
			Q_EMIT authDataChanged(d->auth = other);
		else
			Q_EMIT signDataChanged(d->sign = other);
		break;
	}
	d->smartcard->reloadCard(token, false);
}

std::vector<unsigned char> QSigner::sign(const std::string &method, const std::vector<unsigned char> &digest ) const
{
	#define throwException(msg, code) { \
		Exception e(__FILE__, __LINE__, (msg).toStdString()); \
		e.setCode(code); \
		throw e; \
	}

	if(!d->lock.tryLockForWrite(10 * 1000))
		throwException(tr("Signing/decrypting is already in progress another window."), Exception::General)

	if( d->sign.cert().isNull() )
	{
		d->lock.unlock();
		throwException(tr("Signing certificate is not selected."), Exception::General)
	}

	auto val = QCryptoBackend::getBackend(qApp->signer()->tokenauth());
	if (!val.value()) {
		switch(val.error()) {
		case QCryptoBackend::PinCanceled:
			throwException((tr("Failed to login token") + ' ' + QCryptoBackend::errorString(val.error())), Exception::PINCanceled);
		case QCryptoBackend::PinLocked:
			throwException((tr("Failed to login token") + ' ' + QCryptoBackend::errorString(val.error())), Exception::PINLocked);
		default:
			throwException((tr("Failed to login token") + ' ' + QCryptoBackend::errorString(val.error())), Exception::PINFailed);
		}
	}
	std::unique_ptr<QCryptoBackend> backend(val.value());

	QByteArray sig = waitFor(&QCryptoBackend::sign, backend.get(),
		methodToNID(method), QByteArray::fromRawData((const char*)digest.data(), int(digest.size())));
	if (sig.isEmpty())
		throwException(tr("Failed to sign document"), Exception::General)
	return {sig.constBegin(), sig.constEnd()};
}

QSmartCard * QSigner::smartcard() const { return d->smartcard; }
TokenData QSigner::tokenauth() const { return d->auth; }
TokenData QSigner::tokensign() const { return d->sign; }

QString
QSigner::getLastErrorStr() const
{
	return "Backend error";
}
