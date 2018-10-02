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
#include <QtCore/QLoggingCategory>
#include <QtCore/QScopedPointer>
#include <QtNetwork/QSslKey>
#include <QtWidgets/QApplication>

#include <thread>

Q_LOGGING_CATEGORY(CLog, "qdigidoc4.QSmartCard")

const QHash<QByteArray,QSmartCardData::CardVersion> QSmartCardDataPrivate::atrList = {
	{"3BFE1800008031FE454573744549442076657220312E30A8", QSmartCardData::VER_3_4}, /*ESTEID_V3_COLD_ATR*/
	{"3BFE1800008031FE45803180664090A4162A00830F9000EF", QSmartCardData::VER_3_4}, /*ESTEID_V3_WARM_ATR / ESTEID_V35_WARM_ATR*/
	{"3BFA1800008031FE45FE654944202F20504B4903", QSmartCardData::VER_3_5}, /*ESTEID_V35_COLD_ATR*/
};

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


QSharedPointer<QPCSCReader> QSmartCardPrivate::connect(const QString &reader)
{
	qCDebug(CLog) << "Connecting to reader" << reader;
	QSharedPointer<QPCSCReader> r(new QPCSCReader(reader, &QPCSC::instance()));
	if(!r->connect() || !r->beginTransaction())
		r.clear();
	return r;
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
	if(Settings::language() == QLatin1String("en")) return 0x0409;
	if(Settings::language() == QLatin1String("et")) return 0x0425;
	if(Settings::language() == QLatin1String("ru")) return 0x0419;
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
:	QObject(parent)
,	d(new QSmartCardPrivate)
{
	d->t.d->card = QStringLiteral("loading");
}

QSmartCard::~QSmartCard()
{
	delete d;
}

QSmartCard::ErrorType QSmartCard::change(QSmartCardData::PinType type, QWidget* parent, const QString &newpin, const QString &pin, const QString &title, const QString &bodyText)
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
		p.reset(new PinPopup(PinDialog::PinFlags(flags|PinDialog::PinpadFlag), title, nullptr, parent, bodyText));

		std::thread([&]{
			Q_EMIT p->startTimer();
			result = reader->transferCTL(cmd, false, d->language(), QSmartCardData::minPinLen(type));
			Q_EMIT p->finished(0);
		}).detach();
		p->exec();
	}
	else
		result = reader->transfer(cmd + pin.toUtf8() + newpin.toUtf8());

	return d->handlePinResult(reader.data(), result, true);
}

QSmartCardData QSmartCard::data() const { return d->t; }

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

void QSmartCard::reloadCard(const QString &card, bool isCardId)
{
	qCDebug(CLog) << "Polling";
	if(!d->t.isNull() && !d->t.card().isEmpty() && d->t.card() == card)
		return;

	qCDebug(CLog) << "Poll" << card;
	QByteArray cardid = d->READRECORD;
	if(isCardId)
		cardid[2] = QSmartCardData::DocumentId + 1;
	else
		cardid[2] = QSmartCardData::Id + 1;

	// Check available cards
	QString selectedReader;
	const QStringList readers = QPCSC::instance().readers();
	if(![&] {
		for(const QString &name: readers)
		{
			qCDebug(CLog) << "Connecting to reader" << name;
			QScopedPointer<QPCSCReader> reader(new QPCSCReader(name, &QPCSC::instance()));
			if(!reader->isPresent())
				continue;

			if(!QSmartCardDataPrivate::atrList.contains(reader->atr()))
			{
				qCDebug(CLog) << "Unknown ATR" << reader->atr();
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

			bool readerMatched = false;
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
			if(isCardId)
				readerMatched = !nr.isEmpty() && (nr == card);
			else
				readerMatched = !nr.isEmpty() && card.endsWith(QStringLiteral(",") + nr);
			qCDebug(CLog) << "Card id:" << nr;
			if(readerMatched)
			{
				selectedReader = name;
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

	// read card data
	if(!selectedReader.isEmpty() && d->t.isNull())
	{
		qCDebug(CLog) << "Read card" << card << "info";
		if(readCardData(selectedReader))
			qDebug() << "Failed to read card info, try again next round";
	}
}

bool QSmartCard::readCardData(const QString &selectedReader)
{
	bool tryAgain = false;
	QSharedPointer<QPCSCReader> reader(d->connect(selectedReader));
	if(!reader.isNull())
	{
		QSharedDataPointer<QSmartCardDataPrivate> t;
		t = d->t.d;
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
			else if(reader->transfer(d->AID35).resultOk())
				t->version = QSmartCardData::VER_3_5;
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
					t->data[QSmartCardData::PersonalDataType(data)] = QDate::fromString(record, QStringLiteral("dd.MM.yyyy"));
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
			for(int i = 0; i < data.data.size(); ++i)
			{
				if(i == 0)
					t->appletVersion = QString::number(quint8(data.data[i]));
				else
					t->appletVersion += QString(QStringLiteral(".%1")).arg(quint8(data.data[i]));
			}
		}

		t->data[QSmartCardData::Email] = t->authCert.subjectAlternativeNames().values(QSsl::EmailEntry).value(0);
		if(!tryAgain)
		{
			d->t.d = t;
			emit dataChanged();
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

QSmartCard::ErrorType QSmartCard::unblock(QSmartCardData::PinType type, QWidget* parent, const QString &pin, const QString &puk, const QString &title, const QString &bodyText)
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

		p.reset(new PinPopup(PinDialog::PinFlags(flags|PinDialog::PinpadFlag), title, nullptr, parent, bodyText));

		std::thread([&]{
			Q_EMIT p->startTimer();
			result = reader->transferCTL(cmd, false, d->language(), QSmartCardData::minPinLen(type));
			Q_EMIT p->finished(0);
		}).detach();
		p->exec();
	}
	else
		result = reader->transfer(cmd + puk.toUtf8() + pin.toUtf8());

	return d->handlePinResult(reader.data(), result, true);
}
