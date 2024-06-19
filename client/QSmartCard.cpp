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
#include "Settings.h"
#include "Utils.h"
#include "dialogs/PinPopup.h"
#include "dialogs/PinUnblock.h"

#include <QtCore/QDateTime>
#include <QtCore/QDebug>
#include <QtCore/QLoggingCategory>
#include <QtCore/QScopedPointer>
#include <QtCore/QTimer>
#include <QtNetwork/QSslKey>

Q_LOGGING_CATEGORY(CLog, "qdigidoc4.QSmartCard")

QSmartCardData::QSmartCardData(): d(new QSmartCardDataPrivate) {}
QSmartCardData::QSmartCardData(const QSmartCardData &other) = default;
QSmartCardData::QSmartCardData(QSmartCardData &&other) Q_DECL_NOEXCEPT = default;
QSmartCardData::~QSmartCardData() = default;
QSmartCardData& QSmartCardData::operator =(const QSmartCardData &other) = default;
QSmartCardData& QSmartCardData::operator =(QSmartCardData &&other) Q_DECL_NOEXCEPT = default;
bool QSmartCardData::operator ==(const QSmartCardData &other) const
{
	return d == other.d || (
		d->card == other.d->card &&
		d->authCert == other.d->authCert &&
		d->signCert == other.d->signCert);
}
bool QSmartCardData::operator !=(const QSmartCardData &other) const { return !operator==(other); }

QString QSmartCardData::card() const { return d->card; }

bool QSmartCardData::isNull() const
{ return d->data.isEmpty() && d->authCert.isNull() && d->signCert.isNull(); }
bool QSmartCardData::isPinpad() const { return d->pinpad; }
bool QSmartCardData::isValid() const
{ return d->data.value(Expiry).toDateTime() >= QDateTime::currentDateTime(); }

QString QSmartCardData::reader() const { return d->reader; }

QVariant QSmartCardData::data(PersonalDataType type) const
{ return d->data.value(type); }
SslCertificate QSmartCardData::authCert() const { return d->authCert; }
SslCertificate QSmartCardData::signCert() const { return d->signCert; }
quint8 QSmartCardData::retryCount(PinType type) const { return d->retry.value(type); }

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
	return {};
}

const QByteArray Card::MASTER_FILE = APDU("00A4000C");
const QByteArray Card::MUTUAL_AUTH = APDU("00880000 00 00");
const QByteArray Card::CHANGE = APDU("00240000 00");
const QByteArray Card::READBINARY = APDU("00B00000 00");
const QByteArray Card::REPLACE = APDU("002C0000 00");
const QByteArray Card::VERIFY = APDU("00200000 00");

QPCSCReader::Result Card::transfer(QPCSCReader *reader, bool verify, const QByteArray &apdu,
	QSmartCardData::PinType type, quint8 newPINOffset, bool requestCurrentPIN)
{
	if(!reader->isPinPad())
		return reader->transfer(apdu);
	quint16 language = 0x0000;
	if(Settings::LANGUAGE == QLatin1String("en")) language = 0x0409;
	else if(Settings::LANGUAGE == QLatin1String("et")) language = 0x0425;
	else if(Settings::LANGUAGE == QLatin1String("ru")) language = 0x0419;
	return waitFor(&QPCSCReader::transferCTL, reader,
		apdu, verify, language, QSmartCardData::minPinLen(type), newPINOffset, requestCurrentPIN);
}


QHash<quint8,QByteArray> Card::parseFCI(const QByteArray &data)
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



const QByteArray IDEMIACard::AID = APDU("00A40400 10 A000000077010800070000FE00000100");
const QByteArray IDEMIACard::AID_OT = APDU("00A4040C 0D E828BD080FF2504F5420415750");
const QByteArray IDEMIACard::AID_QSCD = APDU("00A4040C 10 51534344204170706C69636174696F6E");

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

bool IDEMIACard::isSupported(const QByteArray &atr)
{
	return atr == "3BDB960080B1FE451F830012233F536549440F9000F1";
}

bool IDEMIACard::loadPerso(QPCSCReader *reader, QSmartCardDataPrivate *d) const
{
	if(!reader->transfer(AID) ||
		!reader->transfer(MASTER_FILE))
		return false;
	if(d->data.isEmpty() && reader->transfer(APDU("00A4010C025000")))
	{
		QByteArray cmd = APDU("00A4020C025001");
		for(char data = 1; data <= 8; ++data)
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
			case 2: d->data[QSmartCardData::FirstName] = record; break;
			case 4: d->data[QSmartCardData::Citizen] = record; break;
			case 5:
				if(!record.isEmpty())
				{
					QStringList data = record.split(' ');
					if(data.size() > 3)
						data.removeLast();
					d->data[QSmartCardData::BirthDate] = QDate::fromString(data.join('.'), QStringLiteral("dd.MM.yyyy"));
				}
				break;
			case 6: d->data[QSmartCardData::Id] = record; break;
			case 7: d->data[QSmartCardData::DocumentId] = record; break;
			case 8: d->data[QSmartCardData::Expiry] = QDateTime::fromString(record, QStringLiteral("dd MM yyyy")).addDays(1).addSecs(-1); break;
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
		QHash<quint8,QByteArray> fci = parseFCI(data.data);
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
	if(d->authCert.isNull())
		d->authCert = readCert(APDU("00A4010C02ADF1"), APDU("00A4020402340100"));
	if(d->signCert.isNull())
		d->signCert = readCert(APDU("00A4010C02ADF2"), APDU("00A4020402341F00"));

	if(readFailed)
		return false;
	return updateCounters(reader, d);
}

QByteArray IDEMIACard::pinTemplate(const QString &pin)
{
	QByteArray result = pin.toUtf8();
	result += QByteArray(12 - result.size(), char(0xFF));
	return result;
}

QPCSCReader::Result IDEMIACard::replace(QPCSCReader *reader, QSmartCardData::PinType type, const QString &puk_, const QString &pin_) const
{
	auto result = reader->transfer(AID);
	if(!result)
		return result;

	QByteArray puk = pinTemplate(puk_);
	QByteArray cmd = VERIFY;
	cmd[3] = 2;
	cmd[4] = char(puk.size());
	result = transfer(reader, true, cmd + puk, type, 0, true);
	if(!result)
		return result;

	cmd = Card::REPLACE;
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
		return {};
	QByteArray cmd = MUTUAL_AUTH;
	cmd[4] = char(std::min<size_t>(size_t(dgst.size()), 0x30));
	cmd.insert(5, dgst.left(0x30));
	return reader->transfer(cmd).data;
}

bool IDEMIACard::updateCounters(QPCSCReader *reader, QSmartCardDataPrivate *d) const
{
	reader->transfer(AID);
	if(auto data = reader->transfer(APDU("00CB3FFF 0A 4D087006BF810102A08000")))
		d->retry[QSmartCardData::Pin1Type] = quint8(data.data[13]);
	if(auto data = reader->transfer(APDU("00CB3FFF 0A 4D087006BF810202A08000")))
		d->retry[QSmartCardData::PukType] = quint8(data.data[13]);
	reader->transfer(AID_QSCD);
	if(auto data = reader->transfer(APDU("00CB3FFF 0A 4D087006BF810502A08000")))
		d->retry[QSmartCardData::Pin2Type] = quint8(data.data[13]);
	return true;
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
	case 0x6985:
	case 0x6A80: return QSmartCard::OldNewPinSameError;
	default: return QSmartCard::UnknownError;
	}
}



QSmartCard::QSmartCard(QObject *parent)
:	QObject(parent)
,	d(new Private)
{
}

QSmartCard::~QSmartCard() = default;

QSmartCard::ErrorType QSmartCard::change(QSmartCardData::PinType type, QWidget* parent, const QString &newpin, const QString &pin, const QString &title, const QString &bodyText)
{
	PinPopup::PinFlags flags = {};
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
		p.reset(new PinPopup(PinPopup::PinFlags(flags|PinPopup::PinpadChangeFlag), title, {}, parent, bodyText));
		p->open();
	}
	return d->handlePinResult(reader.data(), d->card->change(reader.data(), type, pin, newpin), true);
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
		p.reset(new PinUnblock(isForgotPin ? PinUnblock::ChangePinWithPuk : PinUnblock::UnBlockPinWithPuk, parent, type,
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
		textBody = isForgotPin ?
			tr("To change %1 code with the PUK code on a PinPad reader the PUK code has to be entered first and then the %1 code twice.").arg(QSmartCardData::typeString(type)) :
			tr("To unblock the %1 code on a PinPad reader the PUK code has to be entered first and then the %1 code twice.").arg(QSmartCardData::typeString(type));
	}
	return unblock(type, parent, newPin, puk, title, textBody);
}

void QSmartCard::reloadCounters()
{
	QMetaObject::invokeMethod(this, [this] { reloadCard(d->token, true); });
}

void QSmartCard::reloadCard(const TokenData &token, bool reloadCounters)
{
	qCDebug(CLog) << "Polling";
	d->token = token;
	if(!reloadCounters && !d->t.isNull() && !d->t.card().isEmpty() && d->t.card() == token.card())
		return;

	// check if selected card is same as signer
	if(!d->t.card().isEmpty() && token.card() != d->t.card())
		d->t.d = new QSmartCardDataPrivate();

	// select signer card
	if(d->t.card().isEmpty() || d->t.card() != token.card())
	{
		QSharedDataPointer<QSmartCardDataPrivate> t = d->t.d;
		t->card = token.card();
		t->data.clear();
		t->authCert.clear();
		t->signCert.clear();
		d->t.d = std::move(t);
	}

	if(!reloadCounters && (!d->t.isNull() || token.reader().isEmpty()))
		return;

	QString reader = token.reader();
	if(token.reader().endsWith(QStringLiteral("..."))) {
		for(const QString &test: QPCSC::instance().readers()) {
			if(test.startsWith(token.reader().left(token.reader().size() - 3)))
				reader = test;
		}
	}

	qCDebug(CLog) << "Read" << reader;
	QScopedPointer<QPCSCReader> selectedReader(new QPCSCReader(reader, &QPCSC::instance()));
	if(!selectedReader->connect() || !selectedReader->beginTransaction())
		return;

	if(!IDEMIACard::isSupported(selectedReader->atr())) {
		qDebug() << "Unsupported card";
		return;
	}

	qCDebug(CLog) << "Read card" << token.card() << "info";
	QSharedDataPointer<QSmartCardDataPrivate> t;
	t = d->t.d;
	t->reader = selectedReader->name();
	t->pinpad = selectedReader->isPinPad();
	d->card.reset();
	d->card = std::make_unique<IDEMIACard>();
	if(d->card->loadPerso(selectedReader.data(), t))
	{
		d->t.d = std::move(t);
		emit dataChanged(d->t);
	}
	else
		qDebug() << "Failed to read card info, try again next round";
}

TokenData QSmartCard::tokenData() const { return d->token; }

QSmartCard::ErrorType QSmartCard::unblock(QSmartCardData::PinType type, QWidget* parent, const QString &pin, const QString &puk, const QString &title, const QString &bodyText)
{
	PinPopup::PinFlags flags = {};
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
		p.reset(new PinPopup(PinPopup::PinFlags(flags|PinPopup::PinpadChangeFlag), title, {}, parent, bodyText));
		p->open();
	}
	return d->handlePinResult(reader.data(), d->card->replace(reader.data(), type, puk, pin), true);
}
