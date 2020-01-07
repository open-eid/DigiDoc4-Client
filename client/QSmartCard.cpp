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

#include <common/Common.h>

#include <QtCore/QDateTime>
#include <QtCore/QDebug>
#include <QtCore/QLoggingCategory>
#include <QtCore/QScopedPointer>
#include <QtNetwork/QSslKey>
#include <QtWidgets/QApplication>

#include <thread>

Q_LOGGING_CATEGORY(CLog, "qdigidoc4.QSmartCard")

QSmartCardData::QSmartCardData(): d(new QSmartCardDataPrivate) {}
QSmartCardData::QSmartCardData(const QSmartCardData &other) = default;
QSmartCardData::QSmartCardData(QSmartCardData &&other) Q_DECL_NOEXCEPT: d(std::move(other.d)) {}
QSmartCardData::~QSmartCardData() = default;
QSmartCardData& QSmartCardData::operator =(const QSmartCardData &other) = default;
QSmartCardData& QSmartCardData::operator =(QSmartCardData &&other) Q_DECL_NOEXCEPT { qSwap(d, other.d); return *this; }

QString QSmartCardData::card() const { return d->card; }

bool QSmartCardData::isNull() const
{ return d->data.isEmpty() && d->authCert.isNull() && d->signCert.isNull(); }
bool QSmartCardData::isPinpad() const { return d->pinpad; }
bool QSmartCardData::isSecurePinpad() const
{ return d->reader.contains(QLatin1String("EZIO SHIELD"), Qt::CaseInsensitive); }
bool QSmartCardData::isValid() const
{ return d->data.value(Expiry).toDateTime() >= QDateTime::currentDateTime(); }

QString QSmartCardData::reader() const { return d->reader; }

QVariant QSmartCardData::data(PersonalDataType type) const
{ return d->data.value(type); }
SslCertificate QSmartCardData::authCert() const { return d->authCert; }
SslCertificate QSmartCardData::signCert() const { return d->signCert; }
quint8 QSmartCardData::retryCount(PinType type) const { return d->retry.value(type); }
ulong QSmartCardData::usageCount(PinType type) const { return d->usage.value(type); }
QString QSmartCardData::appletVersion() const { return d->appletVersion; }
QSmartCardData::CardVersion QSmartCardData::version() const { return d->version; }

quint8 QSmartCardData::minPinLen(QSmartCardData::PinType type)
{
	switch(type)
	{
	case QSmartCardData::Pin1Type: return 4;
	case QSmartCardData::Pin2Type: return 5;
	case QSmartCardData::PukType: return 8;
	}
	return 4;
}

QString QSmartCardData::typeString(QSmartCardData::PinType type)
{
	switch(type)
	{
	case Pin1Type: return QStringLiteral("PIN1");
	case Pin2Type: return QStringLiteral("PIN2");
	case PukType: return QStringLiteral("PUK");
	}
	return QString();
}



const QByteArray Card::MASTER_FILE = APDU("00A4000C");// 00"); // Compatibilty for some cards
const QByteArray Card::MUTUAL_AUTH = APDU("00880000 00 00");
const QByteArray Card::CHANGE = APDU("00240000 00");
const QByteArray Card::READBINARY = APDU("00B00000 00");
const QByteArray Card::READRECORD = APDU("00B20004 00");
const QByteArray Card::REPLACE = APDU("002C0000 00");
const QByteArray Card::VERIFY = APDU("00200000 00");

QPCSCReader::Result Card::transfer(QPCSCReader *reader, bool verify, const QByteArray &apdu,
	QSmartCardData::PinType type, quint8 newPINOffset, bool requestCurrentPIN) const
{
	if(!reader->isPinPad())
		return reader->transfer(apdu);
	quint16 language = 0x0000;
	if(Common::language() == QLatin1String("en")) language = 0x0409;
	else if(Common::language() == QLatin1String("et")) language = 0x0425;
	else if(Common::language() == QLatin1String("ru")) language = 0x0419;
	QPCSCReader::Result result;
	QEventLoop l;
	std::thread([&]{
		result = reader->transferCTL(apdu, verify, language, QSmartCardData::minPinLen(type), newPINOffset, requestCurrentPIN);
		l.quit();
	}).detach();
	l.exec();
	return result;
}



const QByteArray EstEIDCard::AID30 = APDU("00A40400 10 D2330000010000010000000000000000");
const QByteArray EstEIDCard::AID34 = APDU("00A40400 0E F04573744549442076657220312E");
const QByteArray EstEIDCard::AID35 = APDU("00A40400 0F D23300000045737445494420763335");
const QByteArray EstEIDCard::UPDATER_AID = APDU("00A40400 0A D2330000005550443101");
const QByteArray EstEIDCard::ESTEIDDF = APDU("00A4010C 02 EEEE");
const QByteArray EstEIDCard::PERSONALDATA = APDU("00A4020C 02 5044");
QTextCodec* EstEIDCard::codec = QTextCodec::codecForName("Windows-1252");

QString EstEIDCard::cardNR(QPCSCReader *reader)
{
	if(!reader->transfer(MASTER_FILE))
	{	// Master file selection failed, test if it is updater applet
		if(!reader->transfer(UPDATER_AID))
			return QString(); // Updater applet not found
		if(!reader->transfer(MASTER_FILE))
		{	//Found updater applet but cannot select master file, select back 3.5
			reader->transfer(AID35);
			return QString();
		}
	}
	if(!reader->transfer(ESTEIDDF) ||
		!reader->transfer(PERSONALDATA))
		return QString();
	QByteArray cardid = READRECORD;
	cardid[2] = 8;
	QPCSCReader::Result result = reader->transfer(cardid);
	return codec->toUnicode(result.data);
}

QPCSCReader::Result EstEIDCard::change(QPCSCReader *reader, QSmartCardData::PinType type, const QString &pin_, const QString &newpin_) const
{
	QByteArray cmd = CHANGE;
	QByteArray newpin = newpin_.toUtf8();
	QByteArray pin = pin_.toUtf8();
	cmd[3] = type == QSmartCardData::PukType ? 0 : type;
	cmd[4] = char(pin.size() + newpin.size());
	return transfer(reader, false, cmd + pin + newpin, type, quint8(pin.size()), true);
}

QSmartCardData::CardVersion EstEIDCard::isSupported(const QByteArray &atr)
{
	static const QHash<QByteArray,QSmartCardData::CardVersion> atrList{
		{"3BFE1800008031FE454573744549442076657220312E30A8", QSmartCardData::VER_3_4}, /*ESTEID_V3_COLD_ATR*/
		{"3BFE1800008031FE45803180664090A4162A00830F9000EF", QSmartCardData::VER_3_4}, /*ESTEID_V3_WARM_ATR / ESTEID_V35_WARM_ATR*/
		{"3BFA1800008031FE45FE654944202F20504B4903", QSmartCardData::VER_3_5}, /*ESTEID_V35_COLD_ATR*/
	};
	return atrList.value(atr, QSmartCardData::VER_INVALID);
}

bool EstEIDCard::loadPerso(QPCSCReader *reader, QSmartCardDataPrivate *d) const
{
	static const QByteArray AUTHCERT = APDU("00A40200 02 AACE");
	static const QByteArray SIGNCERT = APDU("00A40200 02 DDCE");
	static const QByteArray APPLETVER = APDU("00CA0100 00");

	d->version = isSupported(reader->atr());
	if(reader->transfer(AID30).resultOk())
		d->version = QSmartCardData::VER_3_0;
	else if(reader->transfer(AID34).resultOk())
		d->version = QSmartCardData::VER_3_4;
	else if(reader->transfer(UPDATER_AID).resultOk())
	{
		d->version = QSmartCardData::CardVersion(d->version|QSmartCardData::VER_HASUPDATER);
		//Prefer EstEID applet when if it is usable
		if(!reader->transfer(AID35) ||
			!reader->transfer(MASTER_FILE))
		{
			reader->transfer(UPDATER_AID);
			d->version = QSmartCardData::VER_USABLEUPDATER;
		}
	}
	else if(reader->transfer(AID35).resultOk())
		d->version = QSmartCardData::VER_3_5;
	if(reader->transfer(MASTER_FILE).resultOk() &&
		reader->transfer(ESTEIDDF).resultOk() &&
		reader->transfer(PERSONALDATA).resultOk())
	{
		QByteArray cmd = READRECORD;
		for(char data = QSmartCardData::SurName; data != QSmartCardData::Comment4; ++data)
		{
			cmd[2] = data;
			QPCSCReader::Result result = reader->transfer(cmd);
			if(!result)
				return false;
			QString record = codec->toUnicode(result.data.trimmed());
			if(record == QChar(0))
				record.clear();
			switch(data)
			{
			case QSmartCardData::BirthDate:
			case QSmartCardData::Expiry:
			case QSmartCardData::IssueDate:
				d->data[QSmartCardData::PersonalDataType(data)] = QDate::fromString(record, QStringLiteral("dd.MM.yyyy"));
				break;
			default:
				d->data[QSmartCardData::PersonalDataType(data)] = record;
				break;
			}
		}
	}
	QPCSCReader::Result data = reader->transfer(APPLETVER);
	if(data.resultOk())
	{
		for(int i = 0; i < data.data.size(); ++i)
		{
			if(i == 0)
				d->appletVersion = QString::number(quint8(data.data[i]));
			else
				d->appletVersion += QString(QStringLiteral(".%1")).arg(quint8(data.data[i]));
		}
	}
	bool readFailed = false;
	auto readCert = [&](const QByteArray &file) {
		// Workaround some cards, add Le to end
		QPCSCReader::Result data = reader->transfer(file + APDU(reader->protocol() == QPCSCReader::T1 ? "00" : ""));
		if(!data)
			return QSslCertificate();
		QHash<quint8,QByteArray> fci = QSmartCard::parseFCI(data.data);
		int size = fci.contains(0x85) ? quint8(fci[0x85][0]) << 8 | quint8(fci[0x85][1]) : 0x0600;
		QByteArray cert;
		QByteArray cmd = READBINARY;
		while(cert.size() < size)
		{
			cmd[2] = char(cert.size() >> 8);
			cmd[3] = char(cert.size());
			data = reader->transfer(cmd);
			if(!data)
			{
				readFailed = true;
				return QSslCertificate();
			}
			cert += data.data;
		}
		return QSslCertificate(cert, QSsl::Der);
	};
	d->authCert = readCert(AUTHCERT);
	d->signCert = readCert(SIGNCERT);
	if(readFailed)
		return false;
	d->data[QSmartCardData::Email] = d->authCert.subjectAlternativeNames().values(QSsl::EmailEntry).value(0);
	return updateCounters(reader, d);
}

QPCSCReader::Result EstEIDCard::replace(QPCSCReader *reader, QSmartCardData::PinType type, const QString &puk_, const QString &pin_) const
{
	QPCSCReader::Result result;
	if(!reader->isPinPad()) //Verify PUK. Not for pinpad.
	{
		result = verify(reader, QSmartCardData::PukType, puk_);
		if(!result)
			return result;
	}

	// Replace PIN with PUK
	QByteArray pin = pin_.toUtf8();
	QByteArray puk = puk_.toUtf8();
	QByteArray cmd = Card::REPLACE;
	cmd[3] = type;
	cmd[4] = char(puk.size() + pin.size());
	return transfer(reader, false, cmd + puk + pin, type, 0, true);
}

QByteArray EstEIDCard::sign(QPCSCReader *reader, const QByteArray &dgst) const
{
	if(!reader->transfer(APDU("0022F301")) || // 00")) || // Compatibilty for some cards // SECENV1
		!reader->transfer(APDU("002241B8 02 8300"))) //Key reference, 8303801100
		return QByteArray();
	QByteArray cmd = MUTUAL_AUTH;
	cmd[4] = char(dgst.size());
	cmd.insert(5, dgst);
	return reader->transfer(cmd).data;
}

bool EstEIDCard::updateCounters(QPCSCReader *reader, QSmartCardDataPrivate *d) const
{
	static const QByteArray KEYPOINTER = APDU("00A4020C 02 0033");
	static const QByteArray KEYUSAGE = APDU("00A4020C 02 0013");
	static const QByteArray PINRETRY = APDU("00A4020C 02 0016");

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

QPCSCReader::Result EstEIDCard::verify(QPCSCReader *reader, QSmartCardData::PinType type, const QString &pin_) const
{
	QByteArray pin = pin_.toUtf8();
	QByteArray cmd = VERIFY;
	cmd[3] = type == QSmartCardData::PukType ? 0 : type;
	cmd[4] = char(pin.size());
	return transfer(reader, true, cmd + pin, type, 0, true);
}



const QByteArray IDEMIACard::AID = APDU("00A40400 10 A000000077010800070000FE00000100");
const QByteArray IDEMIACard::AID_OT = APDU("00A4040C 0D E828BD080FF2504F5420415750");
const QByteArray IDEMIACard::AID_QSCD = APDU("00A4040C 10 51534344204170706C69636174696F6E");

QString IDEMIACard::cardNR(QPCSCReader *reader)
{
	QPCSCReader::Result result;
	if(!reader->transfer(AID) ||
		!reader->transfer(MASTER_FILE) ||
		!reader->transfer(APDU("00A4010C02D003")) ||
		!(result = reader->transfer(READBINARY)))
		return QString();
	return QString::fromUtf8(result.data.mid(2));
}

QPCSCReader::Result IDEMIACard::change(QPCSCReader *reader, QSmartCardData::PinType type, const QString &pin_, const QString &newpin_) const
{
	QByteArray cmd = CHANGE;
	QByteArray newpin = pinTemplate(newpin_);
	QByteArray pin = pinTemplate(pin_);
	switch (type) {
	case QSmartCardData::Pin1Type:
		reader->transfer(AID);
		cmd[3] = 1;
		break;
	case QSmartCardData::PukType:
		reader->transfer(AID);
		cmd[3] = 2;
		break;
	case QSmartCardData::Pin2Type:
		reader->transfer(AID_QSCD);
		cmd[3] = char(0x85);
		break;
	}
	cmd[4] = char(pin.size() + newpin.size());
	return transfer(reader, false, cmd + pin + newpin, type, quint8(pin.size()), true);
}

QSmartCardData::CardVersion IDEMIACard::isSupported(const QByteArray &atr)
{
	return atr == "3BDB960080B1FE451F830012233F536549440F9000F1" ? QSmartCardData::VER_IDEMIA : QSmartCardData::VER_INVALID;
}

bool IDEMIACard::loadPerso(QPCSCReader *reader, QSmartCardDataPrivate *d) const
{
	d->version = isSupported(reader->atr());
	if(!reader->transfer(AID) ||
		!reader->transfer(MASTER_FILE))
		return false;
	if(reader->transfer(APDU("00A4010C025000")).resultOk())
	{
		QByteArray cmd = APDU("00A4010C025001");
		for(char data = 1; data <= 15; ++data)
		{
			cmd[6] = data;
			if(!reader->transfer(cmd))
				return false;
			QPCSCReader::Result result = reader->transfer(READBINARY);
			if(!result)
				return false;
			QString record = QString::fromUtf8(result.data.trimmed());
			if(record == QChar(0))
				record.clear();
			switch(data)
			{
			case 1: d->data[QSmartCardData::SurName] = record; break;
			case 2: d->data[QSmartCardData::FirstName1] = record; break;
			case 3: d->data[QSmartCardData::Sex] = record; break;
			case 4: d->data[QSmartCardData::Citizen] = record; break;
			case 5:
				if(!record.isEmpty())
				{
					QStringList data = record.split(' ');
					d->data[QSmartCardData::BirthPlace] = data.value(3);
					if(data.size() > 3)
						data.removeLast();
					d->data[QSmartCardData::BirthDate] = QDate::fromString(data.join('.'), QStringLiteral("dd.MM.yyyy"));
				}
				break;
			case 6: d->data[QSmartCardData::Id] = record; break;
			case 7: d->data[QSmartCardData::DocumentId] = record; break;
			case 8: d->data[QSmartCardData::Expiry] = QDate::fromString(record, QStringLiteral("dd MM yyyy")); break;
			case 9: d->data[QSmartCardData::IssueDate] = record; break;
			case 10: d->data[QSmartCardData::ResidencePermit] = record; break;
			case 11: d->data[QSmartCardData::Comment1] = record; break;
			case 12: d->data[QSmartCardData::Comment2] = record; break;
			case 13: d->data[QSmartCardData::Comment3] = record; break;
			case 14: d->data[QSmartCardData::Comment4] = record; break;
			//case 15: d->data[QSmartCardData::Comment5] = record; break;
			default: break;
			}
		}
	}

	bool readFailed = false;
	auto readCert = [&](const QByteArray &file1, const QByteArray &file2) {
		if(!reader->transfer(MASTER_FILE) || !reader->transfer(file1))
		{
			readFailed = true;
			return QSslCertificate();
		}
		QPCSCReader::Result data = reader->transfer(file2);
		if(!data)
			return QSslCertificate();
		QHash<quint8,QByteArray> fci = QSmartCard::parseFCI(data.data);
		int size = fci.contains(0x80) ? quint8(fci[0x80][0]) << 8 | quint8(fci[0x80][1]) : 0x0600;
		QByteArray cert;
		QByteArray cmd = READBINARY;
		while(cert.size() < size)
		{
			cmd[2] = char(cert.size() >> 8);
			cmd[3] = char(cert.size());
			data = reader->transfer(cmd);
			if(!data)
			{
				readFailed = true;
				return QSslCertificate();
			}
			cert += data.data;
		}
		return QSslCertificate(cert, QSsl::Der);
	};
	d->authCert = readCert(APDU("00A4010C02ADF1"), APDU("00A4020402340100"));
	d->signCert = readCert(APDU("00A4010C02ADF2"), APDU("00A4020402341F00"));

	if(readFailed)
		return false;
	if(!d->data[QSmartCardData::Expiry].toDate().isValid())
		d->data[QSmartCardData::Expiry] = d->authCert.expiryDate();
	d->data[QSmartCardData::Email] = d->authCert.subjectAlternativeNames().values(QSsl::EmailEntry).value(0);
	return updateCounters(reader, d);
}

QByteArray IDEMIACard::pinTemplate(const QString &pin) const
{
	QByteArray result = pin.toUtf8();
	result += QByteArray(12 - result.size(), char(0xFF));
	return result;
}

QPCSCReader::Result IDEMIACard::replace(QPCSCReader *reader, QSmartCardData::PinType type, const QString &puk, const QString &pin_) const
{
	QPCSCReader::Result result = verify(reader, QSmartCardData::PukType, puk);
	if(!result)
		return result;

	QByteArray cmd = Card::REPLACE;
	cmd[2] = 2;
	if(type == QSmartCardData::Pin2Type)
	{
		reader->transfer(IDEMIACard::AID_QSCD);
		cmd[3] = char(0x85);
	}
	else
		cmd[3] = type;
	QByteArray pin = pinTemplate(pin_);
	cmd[4] = char(pin.size());
	return transfer(reader, false, cmd + pin, type, 0, false);
}

QByteArray IDEMIACard::sign(QPCSCReader *reader, const QByteArray &dgst) const
{
	if(!reader->transfer(AID_OT) ||
		!reader->transfer(APDU("002241A4 09 8004FF200800840181")))
		return QByteArray();
	QByteArray cmd = MUTUAL_AUTH;
	cmd[4] = char(std::min<size_t>(size_t(dgst.size()), 0x30));
	cmd.insert(5, dgst.left(0x30));
	return reader->transfer(cmd).data;
}

bool IDEMIACard::updateCounters(QPCSCReader *reader, QSmartCardDataPrivate *d) const
{
	d->usage[QSmartCardData::Pin1Type] = 0;
	d->usage[QSmartCardData::Pin2Type] = 0;
	reader->transfer(AID);
	QPCSCReader::Result data = reader->transfer(APDU("00CB3FFF 0A 4D087006BF810102A08000"));
	if(data.resultOk())
		d->retry[QSmartCardData::Pin1Type] = quint8(data.data[13]);
	data = reader->transfer(APDU("00CB3FFF 0A 4D087006BF810202A08000"));
	if(data.resultOk())
		d->retry[QSmartCardData::PukType] = quint8(data.data[13]);
	reader->transfer(AID_QSCD);
	data = reader->transfer(APDU("00CB3FFF 0A 4D087006BF810502A08000"));
	if(data.resultOk())
		d->retry[QSmartCardData::Pin2Type] = quint8(data.data[13]);
	return true;
}

QPCSCReader::Result IDEMIACard::verify(QPCSCReader *reader, QSmartCardData::PinType type, const QString &pin_) const
{
	QByteArray pin = pinTemplate(pin_);
	QByteArray cmd = VERIFY;
	switch (type) {
	case QSmartCardData::Pin1Type:
		reader->transfer(AID);
		cmd[3] = 1;
		break;
	case QSmartCardData::PukType:
		reader->transfer(AID);
		cmd[3] = 2;
		break;
	case QSmartCardData::Pin2Type:
		reader->transfer(AID_QSCD);
		cmd[3] = char(0x85);
		break;
	}
	cmd[4] = char(pin.size());
	return transfer(reader, true, cmd + pin, type, 0, true);
}



QSharedPointer<QPCSCReader> QSmartCard::Private::connect(const QString &reader)
{
	qCDebug(CLog) << "Connecting to reader" << reader;
	QSharedPointer<QPCSCReader> r(new QPCSCReader(reader, &QPCSC::instance()));
	if(!r->connect() || !r->beginTransaction())
		r.clear();
	return r;
}

QSmartCard::ErrorType QSmartCard::Private::handlePinResult(QPCSCReader *reader, const QPCSCReader::Result &response, bool forceUpdate)
{
	if(!response || forceUpdate)
		card->updateCounters(reader, t.d);
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



QSmartCard::QSmartCard(QObject *parent)
:	QObject(parent)
,	d(new Private)
{
	d->t.d->card = QStringLiteral("loading");
}

QSmartCard::~QSmartCard()
{
	delete d;
}

QSmartCard::ErrorType QSmartCard::change(QSmartCardData::PinType type, QWidget* parent, const QString &newpin, const QString &pin, const QString &title, const QString &bodyText)
{
	PinPopup::PinFlags flags;
	switch(type)
	{
	case QSmartCardData::Pin1Type: flags = PinPopup::Pin1Type; break;
	case QSmartCardData::Pin2Type: flags = PinPopup::Pin2Type; break;
	case QSmartCardData::PukType: flags = PinPopup::PukType; break;
	default: return UnknownError;
	}
	QCardLocker locker;
	QSharedPointer<QPCSCReader> reader(d->connect(d->t.reader()));
	if(!reader)
		return UnknownError;

	QScopedPointer<PinPopup> p;
	if(d->t.isPinpad())
	{
		p.reset(new PinPopup(PinPopup::PinFlags(flags|PinPopup::PinpadChangeFlag), title, nullptr, parent, bodyText));
		p->open();
	}
	return d->handlePinResult(reader.data(), d->card->change(reader.data(), type, pin, newpin), true);
}

QSmartCardData QSmartCard::data() const { return d->t; }

QHash<quint8,QByteArray> QSmartCard::parseFCI(const QByteArray &data)
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

QSmartCard::ErrorType QSmartCard::pinChange(QSmartCardData::PinType type, QWidget* parent)
{
	QScopedPointer<PinUnblock> p;
	QByteArray oldPin, newPin;
	QString title, textBody;

	if (!d->t.isPinpad())
	{
		p.reset(new PinUnblock(PinUnblock::PinChange, parent, type, d->t.retryCount(type), d->t.data(QSmartCardData::BirthDate).toDate(),
			d->t.data(QSmartCardData::Id).toString()));
		if (!p->exec())
			return CancelError;
		oldPin = p->firstCodeText().toUtf8();
		newPin = p->newCodeText().toUtf8();
	}
	else
	{
		SslCertificate cert = d->t.authCert();
		title = cert.toString(cert.showCN() ? QStringLiteral("<b>CN,</b> serialNumber") : QStringLiteral("<b>GN SN,</b> serialNumber"));
		textBody = tr("To change %1 on a PinPad reader the old %1 code has to be entered first and then the new %1 code twice.").arg(QSmartCardData::typeString(type));
	}
	return change(type, parent, newPin, oldPin, title, textBody);
}

QSmartCard::ErrorType QSmartCard::pinUnblock(QSmartCardData::PinType type, bool isForgotPin, QWidget* parent)
{
	QScopedPointer<PinUnblock> p;
	QByteArray puk, newPin;
	QString title, textBody;

	if (!d->t.isPinpad())
	{
		p.reset(new PinUnblock((isForgotPin) ? PinUnblock::ChangePinWithPuk : PinUnblock::UnBlockPinWithPuk, parent, type,
			d->t.retryCount(QSmartCardData::PukType), d->t.data(QSmartCardData::BirthDate).toDate(), d->t.data(QSmartCardData::Id).toString()));
		if (!p->exec())
			return CancelError;
		puk = p->firstCodeText().toUtf8();
		newPin = p->newCodeText().toUtf8();
	}
	else
	{
		SslCertificate cert = d->t.authCert();
		title = cert.toString(cert.showCN() ? QStringLiteral("<b>CN,</b> serialNumber") : QStringLiteral("<b>GN SN,</b> serialNumber"));
		textBody = (isForgotPin) ?  
			tr("To change %1 code with the PUK code on a PinPad reader the PUK code has to be entered first and then the %1 code twice.").arg(QSmartCardData::typeString(type))
			:
			tr("To unblock the %1 code on a PinPad reader the PUK code has to be entered first and then the %1 code twice.").arg(QSmartCardData::typeString(type));
	}
	return unblock(type, parent, newPin, puk, title, textBody);
}

void QSmartCard::reload() { selectCard(d->t.card());  }

void QSmartCard::reloadCard(const QString &card)
{
	qCDebug(CLog) << "Polling";
	if(!d->t.isNull() && !d->t.card().isEmpty() && d->t.card() == card)
		return;

	qCDebug(CLog) << "Poll" << card;
	// Check available cards
	QScopedPointer<QPCSCReader> selectedReader;
	const QStringList readers = QPCSC::instance().readers();
	if(![&] {
		for(const QString &name: readers)
		{
			qCDebug(CLog) << "Connecting to reader" << name;
			QScopedPointer<QPCSCReader> reader(new QPCSCReader(name, &QPCSC::instance()));
			if(!reader->isPresent())
				continue;
			switch(reader->connectEx())
			{
			case 0x8010000CL: continue; //SCARD_E_NO_SMARTCARD
			case 0:
				if(reader->beginTransaction())
					break;
			default: return false;
			}
			QString nr;
			if(IDEMIACard::isSupported(reader->atr()) != QSmartCardData::VER_INVALID)
				nr = IDEMIACard::cardNR(reader.data());
			else if(EstEIDCard::isSupported(reader->atr()) != QSmartCardData::VER_INVALID)
				nr = EstEIDCard::cardNR(reader.data());
			else
				continue;
			if(nr.isEmpty())
				return false;
			qCDebug(CLog) << "Card id:" << nr;
			if(!nr.isEmpty() && nr == card)
			{
				selectedReader.swap(reader);
				return true;
			}
		}
		return true;
	}())
	{
		qCDebug(CLog) << "Failed to poll card, try again next round";
		return;
	}

	// check if selected card is same as signer
	if(!d->t.card().isEmpty() && card != d->t.card())
		d->t.d = new QSmartCardDataPrivate();

	// select signer card
	if(d->t.card().isEmpty() || d->t.card() != card)
	{
		QSharedDataPointer<QSmartCardDataPrivate> t = d->t.d;
		t->card = card;
		t->data.clear();
		t->authCert = QSslCertificate();
		t->signCert = QSslCertificate();
		d->t.d = t;
	}

	if(!selectedReader || !d->t.isNull())
		return;

	qCDebug(CLog) << "Read card" << card << "info";
	QSharedDataPointer<QSmartCardDataPrivate> t;
	t = d->t.d;
	t->reader = selectedReader->name();
	t->pinpad = selectedReader->isPinPad();
	delete d->card;
	if(IDEMIACard::isSupported(selectedReader->atr()) == QSmartCardData::VER_IDEMIA)
		d->card = new IDEMIACard();
	else
		d->card = new EstEIDCard();
	if(d->card->loadPerso(selectedReader.data(), t))
	{
		d->t.d = t;
		emit dataChanged();
	}
	else
		qDebug() << "Failed to read card info, try again next round";
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

QSmartCard::ErrorType QSmartCard::unblock(QSmartCardData::PinType type, QWidget* parent, const QString &pin, const QString &puk, const QString &title, const QString &bodyText)
{
	PinPopup::PinFlags flags;
	switch(type)
	{
	case QSmartCardData::Pin1Type: flags = PinPopup::Pin1Type;	break;
	case QSmartCardData::Pin2Type: flags = PinPopup::Pin2Type;	break;
	default: return UnknownError;
	}
	QCardLocker locker;
	QSharedPointer<QPCSCReader> reader(d->connect(d->t.reader()));
	if(!reader)
		return UnknownError;

	QScopedPointer<PinPopup> p;
	if(d->t.isPinpad())
	{
		p.reset(new PinPopup(PinPopup::PinFlags(flags|PinPopup::PinpadChangeFlag), title, nullptr, parent, bodyText));
		p->open();
	}
	return d->handlePinResult(reader.data(), d->card->replace(reader.data(), type, puk, pin), true);
}
