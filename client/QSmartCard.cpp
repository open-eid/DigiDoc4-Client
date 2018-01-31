/*
 * QEstEidUtil
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

#include "QSmartCard_p.h"
#include "QCardLock.h"
#include "dialogs/PinPopup.h"
#include "dialogs/PinUnblock.h"

#include <common/IKValidator.h>
#include <common/PinDialog.h>
#include <common/Settings.h>

#include <QtCore/QDateTime>
#include <QtCore/QDebug>
#include <QtCore/QScopedPointer>
#include <QtNetwork/QSslKey>
#include <QtWidgets/QApplication>

#include <openssl/evp.h>
#include <thread>

const QHash<QByteArray,QSmartCardData::CardVersion> QSmartCardDataPrivate::atrList = {
	{"3BFE9400FF80B1FA451F034573744549442076657220312E3043", QSmartCardData::VER_1_0}, /*ESTEID_V1_COLD_ATR*/
	{"3B6E00FF4573744549442076657220312E30", QSmartCardData::VER_1_0}, /*ESTEID_V1_WARM_ATR*/
	{"3BDE18FFC080B1FE451F034573744549442076657220312E302B", QSmartCardData::VER_1_0_2007}, /*ESTEID_V1_2007_COLD_ATR*/
	{"3B5E11FF4573744549442076657220312E30", QSmartCardData::VER_1_0_2007}, /*ESTEID_V1_2007_WARM_ATR*/
	{"3B6E00004573744549442076657220312E30", QSmartCardData::VER_1_1}, /*ESTEID_V1_1_COLD_ATR*/
	{"3BFE1800008031FE454573744549442076657220312E30A8", QSmartCardData::VER_3_4}, /*ESTEID_V3_COLD_DEV1_ATR*/
	{"3BFE1800008031FE45803180664090A4561B168301900086", QSmartCardData::VER_3_4}, /*ESTEID_V3_WARM_DEV1_ATR*/
	{"3BFE1800008031FE45803180664090A4162A0083019000E1", QSmartCardData::VER_3_4}, /*ESTEID_V3_WARM_DEV2_ATR*/
	{"3BFE1800008031FE45803180664090A4162A00830F9000EF", QSmartCardData::VER_3_4}, /*ESTEID_V3_WARM_DEV3_ATR*/
	{"3BF9180000C00A31FE4553462D3443432D303181", QSmartCardData::VER_3_5}, /*ESTEID_V35_COLD_DEV1_ATR*/
	{"3BF81300008131FE454A434F5076323431B7", QSmartCardData::VER_3_5}, /*ESTEID_V35_COLD_DEV2_ATR*/
	{"3BFA1800008031FE45FE654944202F20504B4903", QSmartCardData::VER_3_5}, /*ESTEID_V35_COLD_DEV3_ATR*/
	{"3BFE1800008031FE45803180664090A4162A00830F9000EF", QSmartCardData::VER_3_5}, /*ESTEID_V35_WARM_ATR*/
	{"3BFE1800008031FE45803180664090A5102E03830F9000EF", QSmartCardData::VER_3_5}, /*UPDATER_TEST_CARDS*/
};

#if OPENSSL_VERSION_NUMBER < 0x10010000L
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

QSmartCardData::QSmartCardData(): d(new QSmartCardDataPrivate) {}
QSmartCardData::QSmartCardData(const QSmartCardData &other) = default;
QSmartCardData::QSmartCardData(QSmartCardData &&other): d(other.d) {}
QSmartCardData::~QSmartCardData() = default;
QSmartCardData& QSmartCardData::operator =(const QSmartCardData &other) = default;
QSmartCardData& QSmartCardData::operator =(QSmartCardData &&other) { qSwap(d, other.d); return *this; }

QString QSmartCardData::card() const { return d->card; }
QStringList QSmartCardData::cards() const { return d->cards; }

bool QSmartCardData::isNull() const
{ return d->data.isEmpty() && d->authCert.isNull() && d->signCert.isNull(); }
bool QSmartCardData::isPinpad() const { return d->pinpad; }
bool QSmartCardData::isSecurePinpad() const
{ return d->reader.contains("EZIO SHIELD", Qt::CaseInsensitive); }
bool QSmartCardData::isValid() const
{ return d->data.value(Expiry).toDateTime() >= QDateTime::currentDateTime(); }

QString QSmartCardData::reader() const { return d->reader; }
QStringList QSmartCardData::readers() const { return d->readers; }

QVariant QSmartCardData::data(PersonalDataType type) const
{ return d->data.value(type); }
SslCertificate QSmartCardData::authCert() const { return d->authCert; }
SslCertificate QSmartCardData::signCert() const { return d->signCert; }
quint8 QSmartCardData::retryCount(PinType type) const { return d->retry.value(type); }
ulong QSmartCardData::usageCount(PinType type) const { return d->usage.value(type); }
QString QSmartCardData::appletVersion() const { return d->appletVersion; }
QSmartCardData::CardVersion QSmartCardData::version() const { return d->version; }

QString QSmartCardData::typeString(QSmartCardData::PinType type)
{
	switch(type)
	{
	case Pin1Type: return "PIN1";
	case Pin2Type: return "PIN2";
	case PukType: return "PUK";
	}
	return "";
}



QCardInfo::QCardInfo( const QString& id )
: id(id)
, fullName( "Loading ..." )
, loading( true )
{
}

QCardInfo::QCardInfo( const QSmartCardData &scd )
{
	id = scd.data( QSmartCardData::Id ).toString();
	loading = false;
	setFullName( 
		scd.data( QSmartCardData::FirstName1 ).toString(),
		scd.data( QSmartCardData::FirstName2 ).toString(),
		scd.data( QSmartCardData::SurName ).toString()
	);
	setCardType( scd.authCert() );
}

QCardInfo::QCardInfo( const QSmartCardDataPrivate &scdp )
{
	id = scdp.data[ QSmartCardData::Id ].toString();
	loading = false;
	setFullName( 
		scdp.data[ QSmartCardData::FirstName1 ].toString(),
		scdp.data[ QSmartCardData::FirstName2 ].toString(),
		scdp.data[ QSmartCardData::SurName ].toString()
	);
	setCardType( scdp.authCert );
}
	
void QCardInfo::setFullName( const QString &firstName1, const QString &firstName2, const QString &surName )
{
	QStringList firstName = QStringList()
		<< firstName1
		<< firstName2;
	firstName.removeAll( "" );
	QStringList name = QStringList()
		<< firstName
		<< surName;
	name.removeAll( "" );
	fullName = name.join(" ");
}

void QCardInfo::setCardType( const SslCertificate &cert )
{
	if( cert.type() & SslCertificate::DigiIDType )
	{
		cardType = "Digi ID";
	}
	else
	{
		cardType = "ID Card";
	}
}



QSharedPointer<QPCSCReader> QSmartCardPrivate::connect(const QString &reader)
{
	qDebug() << "Connecting to reader" << reader;
	QSharedPointer<QPCSCReader> r(new QPCSCReader(reader, &QPCSC::instance()));
	if(r->connect() && r->beginTransaction())
		return r;
	return QSharedPointer<QPCSCReader>();
}

QSmartCard::ErrorType QSmartCardPrivate::handlePinResult(QPCSCReader *reader, const QPCSCReader::Result &response, bool forceUpdate)
{
	if(!response || forceUpdate)
		updateCounters(reader, t.d);
	switch((quint8(response.SW[0]) << 8) + quint8(response.SW[1]))
	{
	case 0x9000: return QSmartCard::NoError;
	case 0x63C0: return QSmartCard::BlockedError;//pin retry count 0
	case 0x63C1: // Validate error, 1 tries left
	case 0x63C2: // Validate error, 2 tries left
	case 0x63C3: return QSmartCard::ValidateError;
	case 0x6400: // Timeout (SCM)
	case 0x6401: return QSmartCard::CancelError; // Cancel (OK, SCM)
	case 0x6402: return QSmartCard::DifferentError;
	case 0x6403: return QSmartCard::LenghtError;
	case 0x6983: return QSmartCard::BlockedError;
	case 0x6985: return QSmartCard::OldNewPinSameError;
	case 0x6A80: return QSmartCard::OldNewPinSameError;
	default: return QSmartCard::UnknownError;
	}
}

quint16 QSmartCardPrivate::language() const
{
	if(Settings().language() == "en") return 0x0409;
	if(Settings().language() == "et") return 0x0425;
	if(Settings().language() == "ru") return 0x0419;
	return 0x0000;
}

QHash<quint8,QByteArray> QSmartCardPrivate::parseFCI(const QByteArray &data)
{
	QHash<quint8,QByteArray> result;
	for(QByteArray::const_iterator i = data.constBegin(); i != data.constEnd(); ++i)
	{
		quint8 tag(*i), size(*++i);
		result[tag] = QByteArray(i + 1, size);
		switch(tag)
		{
		case 0x6F:
		case 0x62:
		case 0x64:
		case 0xA1: continue;
		default: i += size; break;
		}
	}
	return result;
}

QByteArray QSmartCardPrivate::sign(const unsigned char *dgst, int dgst_len, QSmartCardPrivate *d)
{
	if(!d ||
		!d->reader ||
		!d->reader->transfer(d->SECENV1) ||
		!d->reader->transfer(APDU("002241B8 02 8300"))) //Key reference, 8303801100
		return QByteArray();
	QByteArray cmd = APDU("0088000000"); //calc signature
	cmd[4] = char(dgst_len);
	cmd += QByteArray::fromRawData((const char*)dgst, dgst_len);
	QPCSCReader::Result result = d->reader->transfer(cmd);
	if(!result)
		return QByteArray();
	return result.data;
}

int QSmartCardPrivate::rsa_sign(int type, const unsigned char *m, unsigned int m_len,
		unsigned char *sigret, unsigned int *siglen, const RSA *rsa)
{
	if(type != NID_md5_sha1 || m_len != 36)
		return 0;
	QByteArray result = sign(m, int(m_len), (QSmartCardPrivate*)RSA_get_app_data(rsa));
	if(result.isEmpty())
		return 0;
	*siglen = (unsigned int)result.size();
	memcpy(sigret, result.constData(), size_t(result.size()));
	return 1;
}

ECDSA_SIG* QSmartCardPrivate::ecdsa_do_sign(const unsigned char *dgst, int dgst_len,
		const BIGNUM *, const BIGNUM *, EC_KEY *eckey)
{
#if OPENSSL_VERSION_NUMBER < 0x10010000L
	QSmartCardPrivate *d = (QSmartCardPrivate*)EC_KEY_get_key_method_data(eckey, nullptr, nullptr, nullptr);
#else
	QSmartCardPrivate *d = (QSmartCardPrivate*)EC_KEY_get_ex_data(eckey, 0);
#endif
	QByteArray result = sign(dgst, dgst_len, d);
	if(result.isEmpty())
		return nullptr;
	QByteArray r = result.left(result.size()/2);
	QByteArray s = result.right(result.size()/2);
	ECDSA_SIG *sig = ECDSA_SIG_new();
	ECDSA_SIG_set0(sig,
		BN_bin2bn((const unsigned char*)r.data(), int(r.size()), 0),
		BN_bin2bn((const unsigned char*)s.data(), int(s.size()), 0));
	return sig;
}

bool QSmartCardPrivate::updateCounters(QPCSCReader *reader, QSmartCardDataPrivate *d)
{
	if(!reader->transfer(MASTER_FILE) ||
		!reader->transfer(PINRETRY))
		return false;

	QByteArray cmd = READRECORD;
	for(int i = QSmartCardData::Pin1Type; i <= QSmartCardData::PukType; ++i)
	{
		cmd[2] = char(i);
		QPCSCReader::Result data = reader->transfer(cmd);
		if(!data)
			return false;
		d->retry[QSmartCardData::PinType(i)] = quint8(data.data[5]);
	}

	if(!reader->transfer(ESTEIDDF) ||
		!reader->transfer(KEYPOINTER))
		return false;

	cmd[2] = 1;
	QPCSCReader::Result data = reader->transfer(cmd);
	if(!data)
		return false;

	/*
	 * SIGN1 0100 1
	 * SIGN2 0200 2
	 * AUTH1 1100 3
	 * AUTH2 1200 4
	 */
	quint8 signkey = data.data.at(0x13) == 0x01 && data.data.at(0x14) == 0x00 ? 1 : 2;
	quint8 authkey = data.data.at(0x09) == 0x11 && data.data.at(0x0A) == 0x00 ? 3 : 4;

	if(!reader->transfer(KEYUSAGE))
		return false;

	cmd[2] = char(authkey);
	data = reader->transfer(cmd);
	if(!data)
		return false;
	d->usage[QSmartCardData::Pin1Type] = 0xFFFFFF - ((quint8(data.data[12]) << 16) + (quint8(data.data[13]) << 8) + quint8(data.data[14]));

	cmd[2] = char(signkey);
	data = reader->transfer(cmd);
	if(!data)
		return false;
	d->usage[QSmartCardData::Pin2Type] = 0xFFFFFF - ((quint8(data.data[12]) << 16) + (quint8(data.data[13]) << 8) + quint8(data.data[14]));
	return true;
}




QSmartCard::QSmartCard(QObject *parent)
:	QThread(parent)
,	d(new QSmartCardPrivate)
{
#if OPENSSL_VERSION_NUMBER < 0x10010000L || defined(LIBRESSL_VERSION_NUMBER)
	d->rsamethod.name = "QSmartCard";
	d->rsamethod.rsa_sign = QSmartCardPrivate::rsa_sign;
	ECDSA_METHOD_set_name(d->ecmethod, const_cast<char*>("QSmartCard"));
	ECDSA_METHOD_set_sign(d->ecmethod, QSmartCardPrivate::ecdsa_do_sign);
	ECDSA_METHOD_set_app_data(d->ecmethod, d);
#else
	RSA_meth_set1_name(d->rsamethod, "QSmartCard");
	RSA_meth_set_sign(d->rsamethod, QSmartCardPrivate::rsa_sign);
	EC_KEY_METHOD_set_sign(d->ecmethod, nullptr, nullptr, QSmartCardPrivate::ecdsa_do_sign);
#endif

	d->t.d->readers = QPCSC::instance().readers();
	d->t.d->cards = QStringList() << "loading";
	d->t.d->card = "loading";
}

QSmartCard::~QSmartCard()
{
	d->terminate = true;
	wait();
#if OPENSSL_VERSION_NUMBER >= 0x10010000L
	RSA_meth_free(d->rsamethod);
	EC_KEY_METHOD_free(d->ecmethod);
#else
	ECDSA_METHOD_free(d->ecmethod);
#endif
	delete d;
}

QSmartCard::ErrorType QSmartCard::change(QSmartCardData::PinType type, const QString &newpin, const QString &pin, const QString &title, const QString &bodyText)
{
	PinDialog::PinFlags flags;
	QCardLocker locker;
	QSharedPointer<QPCSCReader> reader(d->connect(d->t.reader()));
	if(!reader)
		return UnknownError;

	QByteArray cmd = d->CHANGE;
	cmd[3] = type == QSmartCardData::PukType ? 0 : type;
	cmd[4] = char(pin.size() + newpin.size());
	QPCSCReader::Result result;
	QScopedPointer<PinPopup> p;

	switch(type)
	{
		case QSmartCardData::Pin1Type: flags = PinDialog::Pin1Type;	break;
		case QSmartCardData::Pin2Type: flags = PinDialog::Pin2Type;	break;
		case QSmartCardData::PukType: flags = PinDialog::PinpadFlag; break;
		default: return UnknownError;
	}
	if(d->t.isPinpad())
	{
		p.reset(new PinPopup(PinDialog::PinFlags(flags|PinDialog::PinpadFlag), title, 0, qApp->activeWindow(),bodyText));

		std::thread([&]{
			Q_EMIT p->startTimer();
			result = reader->transferCTL(cmd, false, d->language(), [&]() -> quint8 {
				switch(type)
				{
				default:
				case QSmartCardData::Pin1Type: return 4;
				case QSmartCardData::Pin2Type: return 5;
				case QSmartCardData::PukType: return 8;
				}
			}());
			Q_EMIT p->finished(0);
		}).detach();
		p->exec();
	}
	else
		result = reader->transfer(cmd + pin.toUtf8() + newpin.toUtf8());

	return d->handlePinResult(reader.data(), result, true);
}

QMap<QString, QSharedPointer<QCardInfo>> QSmartCard::cache() const { return d->cache; };

QSmartCardData QSmartCard::data() const { return d->t; }

Qt::HANDLE QSmartCard::key()
{
	if (d->t.authCert().publicKey().algorithm() == QSsl::Ec)
	{
		EC_KEY *ec = EC_KEY_dup((EC_KEY*)d->t.authCert().publicKey().handle());
		if(!ec)
			return nullptr;
#if OPENSSL_VERSION_NUMBER < 0x10010000L
		EC_KEY_insert_key_method_data(ec, d, nullptr, nullptr, nullptr);
		ECDSA_set_method(ec, d->ecmethod);
#else
		EC_KEY_set_ex_data(ec, 0, d);
		EC_KEY_set_method(ec, d->ecmethod);
#endif
		EVP_PKEY *key = EVP_PKEY_new();
		EVP_PKEY_set1_EC_KEY(key, ec);
		EC_KEY_free(ec);
		return Qt::HANDLE(key);
	}
	else
	{
		RSA *rsa = RSAPublicKey_dup((RSA*)d->t.authCert().publicKey().handle());
		if (!rsa)
			return nullptr;

#if OPENSSL_VERSION_NUMBER < 0x10010000L || defined(LIBRESSL_VERSION_NUMBER)
		RSA_set_method(rsa, &d->rsamethod);
		rsa->flags |= RSA_FLAG_SIGN_VER;
#else
		RSA_set_method(rsa, d->rsamethod);
#endif
		RSA_set_app_data(rsa, d);
		EVP_PKEY *key = EVP_PKEY_new();
		EVP_PKEY_set1_RSA(key, rsa);
		RSA_free(rsa);
		return Qt::HANDLE(key);
	}
}

QSmartCard::ErrorType QSmartCard::pinChange(QSmartCardData::PinType type)
{
	QScopedPointer<PinUnblock> p;
	QByteArray oldPin, newPin;
	QString title, textBody;

	if (!d->t.isPinpad())
	{
		p.reset(new PinUnblock(PinUnblock::PinChange, qApp->activeWindow(), type, d->t.retryCount(type)));
		if (!p->exec())
			return CancelError;
		oldPin = p->firstCodeText().toUtf8();
		newPin = p->newCodeText().toUtf8();
	}
	else
	{
		SslCertificate cert = d->t.authCert();
		title = cert.toString( cert.showCN() ? "<b>CN,</b> serialNumber" : "<b>GN SN,</b> serialNumber" );
		textBody = tr("To change %1 on a PinPad reader the old %1 code has to be entered first and then the new %1 code twice.").arg(QSmartCardData::typeString(type));
	}
	return change(type, newPin, oldPin, title, textBody);
}

QSmartCard::ErrorType QSmartCard::pinUnblock(QSmartCardData::PinType type, bool isForgotPin)
{
	QScopedPointer<PinUnblock> p;
	QByteArray puk, newPin;
	QString title, textBody;

	if (!d->t.isPinpad())
	{
		p.reset(new PinUnblock((isForgotPin) ? PinUnblock::ChangePinWithPuk : PinUnblock::UnBlockPinWithPuk, qApp->activeWindow(), type, d->t.retryCount(QSmartCardData::PukType)));
		if (!p->exec())
			return CancelError;
		puk = p->firstCodeText().toUtf8();
		newPin = p->newCodeText().toUtf8();
	}
	else
	{
		SslCertificate cert = d->t.authCert();
		title = cert.toString( cert.showCN() ? "<b>CN,</b> serialNumber" : "<b>GN SN,</b> serialNumber" );
		textBody = (isForgotPin) ?  
			tr("To change %1 code with the PUK code on a PinPad reader the PUK code has to be entered first and then the %1 code twice.").arg(QSmartCardData::typeString(type))
			:
			tr("To unblock the %1 code on a PinPad reader the PUK code has to be entered first and then the %1 code twice.").arg(QSmartCardData::typeString(type));
	}
	return unblock(type, newPin, puk, title, textBody);
}

QSmartCard::ErrorType QSmartCard::login(QSmartCardData::PinType type)
{
	PinDialog::PinFlags flags = PinDialog::Pin1Type;
	QSslCertificate cert;
	switch(type)
	{
	case QSmartCardData::Pin1Type: flags = PinDialog::Pin1Type; cert = d->t.authCert(); break;
	case QSmartCardData::Pin2Type: flags = PinDialog::Pin2Type; cert = d->t.signCert(); break;
	default: return UnknownError;
	}

	QScopedPointer<PinPopup> p;
	QByteArray pin;
	if(!d->t.isPinpad())
	{
		p.reset(new PinPopup(flags, cert, 0, qApp->activeWindow()));
		if(!p->exec())
			return CancelError;
		pin = p->text().toUtf8();
	}
	else
		p.reset(new PinPopup(PinDialog::PinFlags(flags|PinDialog::PinpadFlag), cert, 0, qApp->activeWindow()));

	QCardLock::instance().exclusiveLock();
	d->reader = d->connect(d->t.reader());
	if(!d->reader)
	{
		QCardLock::instance().exclusiveUnlock();
		return UnknownError;
	}
	QByteArray cmd = d->VERIFY;
	cmd[3] = type;
	cmd[4] = char(pin.size());
	QPCSCReader::Result result;
	if(d->t.isPinpad())
	{
		std::thread([&]{
			Q_EMIT p->startTimer();
			result = d->reader->transferCTL(cmd, true, d->language());
			Q_EMIT p->finished(0);
		}).detach();
		p->exec();
	}
	else
		result = d->reader->transfer(cmd + pin);
	QSmartCard::ErrorType err = d->handlePinResult(d->reader.data(), result, false);
	if(!result)
	{
		d->updateCounters(d->reader.data(), d->t.d);
		d->reader.clear();
		QCardLock::instance().exclusiveUnlock();
	}
	return err;
}

void QSmartCard::logout()
{
	if(d->reader.isNull())
		return;
	d->updateCounters(d->reader.data(), d->t.d);
	d->reader.clear();
	QCardLock::instance().exclusiveUnlock();
}

void QSmartCard::reload() { selectCard(d->t.card());  }

void QSmartCard::run()
{
	QByteArray cardid = d->READRECORD;
	cardid[2] = 8;

	while(!d->terminate)
	{
		if(QCardLock::instance().readTryLock())
		{
			// Get list of available cards
			QMap<QString,QString> cards;
			const QStringList readers = QPCSC::instance().readers();
			if(![&] {
				for(const QString &name: readers)
				{
					qDebug() << "Connecting to reader" << name;
					QScopedPointer<QPCSCReader> reader(new QPCSCReader(name, &QPCSC::instance()));
					if(!reader->isPresent())
						continue;

					if(!QSmartCardDataPrivate::atrList.contains(reader->atr()))
					{
						qDebug() << "Unknown ATR" << reader->atr();
						continue;
					}

					switch(reader->connectEx())
					{
					case 0x8010000CL: continue; //SCARD_E_NO_SMARTCARD
					case 0:
						if(reader->beginTransaction())
							break;
					default: return false;
					}

					QPCSCReader::Result result;
					#define TRANSFERIFNOT(X) result = reader->transfer(X); \
						if(result.err) return false; \
						if(!result)

					TRANSFERIFNOT(d->MASTER_FILE)
					{	// Master file selection failed, test if it is updater applet
						TRANSFERIFNOT(d->UPDATER_AID)
							continue; // Updater applet not found
						TRANSFERIFNOT(d->MASTER_FILE)
						{	//Found updater applet but cannot select master file, select back 3.5
							reader->transfer(d->AID35);
							continue;
						}
					}
					TRANSFERIFNOT(d->ESTEIDDF)
						continue;
					TRANSFERIFNOT(d->PERSONALDATA)
						continue;
					TRANSFERIFNOT(cardid)
						continue;
					QString nr = d->codec->toUnicode(result.data);
					if(!nr.isEmpty())
						cards[nr] = name;
				}
				return true;
			}())
			{
				qDebug() << "Failed to poll card, try again next round";
				QCardLock::instance().readUnlock();
				sleep(5);
				continue;
			}

			// cardlist has changed
			QStringList order = cards.keys();
			std::sort(order.begin(), order.end(), TokenData::cardsOrder);
			bool update = d->t.cards() != order || d->t.readers() != readers;

			// check if selected card is still in slot
			if(!d->t.card().isEmpty() && !order.contains(d->t.card()))
			{
				update = true;
				d->t.d = new QSmartCardDataPrivate();
			}

			d->t.d->cards = order;
			d->t.d->readers = readers;

			// if none is selected select first from cardlist
			if(d->t.card().isEmpty() && !d->t.cards().isEmpty())
			{
				QSharedDataPointer<QSmartCardDataPrivate> t = d->t.d;
				t->card = d->t.cards().first();
				t->data.clear();
				t->authCert = QSslCertificate();
				t->signCert = QSslCertificate();
				d->t.d = t;
				update = true;
				Q_EMIT dataChanged();
			}

			// read card data
			if(d->t.cards().contains(d->t.card()) && d->t.isNull())
			{
				update = true;
				if(readCardData( cards, d->t.card(), true ))
				{
					qDebug() << "Failed to read card info, try again next round";
					update = false;
				}
			}

			// update data if something has changed
			if(update)
			{
				Q_EMIT dataChanged();
			}

			auto added = order.toSet().subtract( d->cache.keys().toSet() );
			for( auto newCard: added )
			{
				readCardData( cards, newCard, false );
			}

			QCardLock::instance().readUnlock();
		}
		sleep(5);
	}
}

bool QSmartCard::readCardData( const QMap<QString,QString> &cards, const QString &card, bool selectedCard )
{
	bool tryAgain = false;
	QSharedPointer<QPCSCReader> reader(d->connect(cards.value(card)));
	if(!reader.isNull())
	{
		QSharedDataPointer<QSmartCardDataPrivate> t;
		QSharedPointer<QSmartCardData> scd;
		if( selectedCard )
		{
			t = d->t.d;
		}
		else
		{
			scd.reset( new QSmartCardData );
			t = scd->d;
		}
		t->reader = reader->name();
		t->pinpad = reader->isPinPad();
		t->version = QSmartCardDataPrivate::atrList.value(reader->atr(), QSmartCardData::VER_INVALID);
		if(t->version > QSmartCardData::VER_1_1)
		{
			if(reader->transfer(d->AID30).resultOk())
				t->version = QSmartCardData::VER_3_0;
			else if(reader->transfer(d->AID34).resultOk())
				t->version = QSmartCardData::VER_3_4;
			else if(reader->transfer(d->UPDATER_AID).resultOk())
			{
				t->version = QSmartCardData::CardVersion(t->version|QSmartCardData::VER_HASUPDATER);
				//Prefer EstEID applet when if it is usable
				if(!reader->transfer(d->AID35) ||
					!reader->transfer(d->MASTER_FILE))
				{
					reader->transfer(d->UPDATER_AID);
					t->version = QSmartCardData::VER_USABLEUPDATER;
				}
			}
		}

		tryAgain = !d->updateCounters(reader.data(), t);
		if(reader->transfer(d->PERSONALDATA).resultOk())
		{
			QByteArray cmd = d->READRECORD;
			for(int data = QSmartCardData::SurName; data != QSmartCardData::Comment4; ++data)
			{
				cmd[2] = char(data + 1);
				QPCSCReader::Result result = reader->transfer(cmd);
				if(!result)
				{
					tryAgain = true;
					break;
				}
				QString record = d->codec->toUnicode(result.data.trimmed());
				if(record == QChar(0))
					record.clear();
				switch(data)
				{
				case QSmartCardData::BirthDate:
				case QSmartCardData::Expiry:
				case QSmartCardData::IssueDate:
					t->data[QSmartCardData::PersonalDataType(data)] = QDate::fromString(record, "dd.MM.yyyy");
					break;
				default:
					t->data[QSmartCardData::PersonalDataType(data)] = record;
					break;
				}
			}
		}

		auto readCert = [&](const QByteArray &file) {
			QPCSCReader::Result data = reader->transfer(file + APDU(reader->protocol() == QPCSCReader::T1 ? "00" : ""));
			if(!data)
				return QSslCertificate();
			QHash<quint8,QByteArray> fci = d->parseFCI(data.data);
			int size = fci.contains(0x85) ? fci[0x85][0] << 8 | fci[0x85][1] : 0x0600;
			QByteArray cert;
			while(cert.size() < size)
			{
				QByteArray cmd = d->READBINARY;
				cmd[2] = char(cert.size() >> 8);
				cmd[3] = char(cert.size());
				data = reader->transfer(cmd);
				if(!data)
				{
					tryAgain = true;
					return QSslCertificate();
				}
				cert += data.data;
			}
			return QSslCertificate(cert, QSsl::Der);
		};
		t->authCert = readCert(d->AUTHCERT);
		t->signCert = readCert(d->SIGNCERT);

		QPCSCReader::Result data = reader->transfer(d->APPLETVER);
		if (data.resultOk())
		{
			if(data.data.size() == 2)
				t->appletVersion = QString("%1.%2").arg(quint8(data.data[0])).arg(quint8(data.data[1]));
			else if (data.data.size() == 3)
				t->appletVersion = QString("%1.%2.%3").arg(quint8(data.data[0])).arg(quint8(data.data[1])).arg(quint8(data.data[2]));
		}

		t->data[QSmartCardData::Email] = t->authCert.subjectAlternativeNames().values(QSsl::EmailEntry).value(0);
		if(t->authCert.type() & SslCertificate::DigiIDType)
		{
			t->data[QSmartCardData::SurName] = t->authCert.toString("SN");
			t->data[QSmartCardData::FirstName1] = t->authCert.toString("GN");
			t->data[QSmartCardData::FirstName2] = QString();
			t->data[QSmartCardData::Id] = t->authCert.subjectInfo("serialNumber");
			t->data[QSmartCardData::BirthDate] = IKValidator::birthDate(t->authCert.subjectInfo("serialNumber"));
			t->data[QSmartCardData::IssueDate] = t->authCert.effectiveDate();
			t->data[QSmartCardData::Expiry] = t->authCert.expiryDate();
		}
		if(tryAgain)
		{
			qDebug() << "Failed to read card info, try again next round";
		}
		else if( !d->cache.contains( card ) )
		{
			qDebug() << "Successfully read data of card " << card;
			d->cache.insert( card, QSharedPointer<QCardInfo>(new QCardInfo( *t )) );
			Q_EMIT dataLoaded();
		}

		if( selectedCard && !tryAgain )
		{
			d->t.d = t;
		}
	}

	return tryAgain;
}

void QSmartCard::selectCard(const QString &card)
{
	QCardLocker locker;
	QSharedDataPointer<QSmartCardDataPrivate> t = d->t.d;
	t->card = card;
	t->data.clear();
	t->authCert = QSslCertificate();
	t->signCert = QSslCertificate();
	d->t.d = t;
	Q_EMIT dataChanged();
}

QSmartCard::ErrorType QSmartCard::unblock(QSmartCardData::PinType type, const QString &pin, const QString &puk, const QString &title, const QString &bodyText)
{
	PinDialog::PinFlags flags;
	QCardLocker locker;
	QSharedPointer<QPCSCReader> reader(d->connect(d->t.reader()));
	if(!reader)
		return UnknownError;

	QByteArray cmd = d->VERIFY;
	QPCSCReader::Result result;
	QScopedPointer<PinPopup> p;

	if(!d->t.isPinpad())
	{
		//Verify PUK. Not for pinpad.
		cmd[3] = 0;
		cmd[4] = char(puk.size());
		result = reader->transfer(cmd + puk.toUtf8());
		if(!result)
			return d->handlePinResult(reader.data(), result, false);
	}
	switch(type)
	{
		case QSmartCardData::Pin1Type: flags = PinDialog::Pin1Type;	break;
		case QSmartCardData::Pin2Type: flags = PinDialog::Pin2Type;	break;
		default: return UnknownError;
	}
	// Make sure pin is locked. ID card is designed so that only blocked PIN could be unblocked with PUK!
	cmd[3] = type;
	cmd[4] = char(pin.size() + 1);
	for(quint8 i = 0; i <= d->t.retryCount(type); ++i)
		reader->transfer(cmd + QByteArray(pin.size(), '0') + QByteArray::number(i));

	//Replace PIN with PUK
	cmd = d->REPLACE;
	cmd[3] = type;
	cmd[4] = char(puk.size() + pin.size());

	if(d->t.isPinpad()) {

		p.reset(new PinPopup(PinDialog::PinFlags(flags|PinDialog::PinpadFlag), title, 0, qApp->activeWindow(),bodyText));

		std::thread([&]{
			Q_EMIT p->startTimer();
			result = reader->transferCTL(cmd, false, d->language(), [&]() -> quint8 {
				switch(type)
				{
				default:
				case QSmartCardData::Pin1Type: return 4;
				case QSmartCardData::Pin2Type: return 5;
				case QSmartCardData::PukType: return 8; // User can not unblock PUK code with application!
				}
			}());
			Q_EMIT p->finished(0);
		}).detach();
		p->exec();
	}
	else
		result = reader->transfer(cmd + puk.toUtf8() + pin.toUtf8());

	return d->handlePinResult(reader.data(), result, true);
}
