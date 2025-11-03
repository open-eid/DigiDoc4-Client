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

#include "IKValidator.h"
#include "Settings.h"
#include "Utils.h"
#include "dialogs/PinPopup.h"
#include "dialogs/PinUnblock.h"
#include "effects/FadeInNotification.h"

#include <QtCore/QDateTime>
#include <QtCore/QLoggingCategory>
#include <QtCore/QScopedPointer>
#include <QtNetwork/QSslKey>

static Q_LOGGING_CATEGORY(CLog, "qdigidoc4.QSmartCard")

QSmartCardData::QSmartCardData(): d(new QSmartCardDataPrivate) {}
QSmartCardData::QSmartCardData(const QSmartCardData &other) = default;
QSmartCardData::QSmartCardData(QSmartCardData &&other) noexcept = default;
QSmartCardData::~QSmartCardData() noexcept = default;
QSmartCardData& QSmartCardData::operator =(const QSmartCardData &other) = default;
QSmartCardData& QSmartCardData::operator =(QSmartCardData &&other) noexcept = default;
bool QSmartCardData::operator ==(const QSmartCardData &other) const
{
	return d == other.d || (
		d->card == other.d->card &&
		d->authCert == other.d->authCert &&
		d->signCert == other.d->signCert);
}

QString QSmartCardData::card() const { return d->card; }

bool QSmartCardData::isNull() const
{ return d->data.isEmpty() && d->authCert.isNull() && d->signCert.isNull(); }
bool QSmartCardData::isPinpad() const { return d->pinpad; }
bool QSmartCardData::isPUKReplacable() const { return d->pukReplacable; }
bool QSmartCardData::isValid() const
{ return d->data.value(Expiry).toDateTime() >= QDateTime::currentDateTime(); }

QString QSmartCardData::reader() const { return d->reader; }

QVariant QSmartCardData::data(PersonalDataType type) const
{ return d->data.value(type); }
SslCertificate QSmartCardData::authCert() const { return d->authCert; }
SslCertificate QSmartCardData::signCert() const { return d->signCert; }
quint8 QSmartCardData::retryCount(PinType type) const { return d->retry.value(type); }
bool QSmartCardData::pinLocked(PinType type) const { return d->locked.value(type, false); }

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


QByteArrayView Card::parseFCI(QByteArrayView data, quint8 expectedTag)
{
	for(auto i = data.begin(); i != data.end();)
	{
		quint8 tag(*i++), size(*i++);
		if(tag == expectedTag)
			return {i, size};
		if((tag & 0x20) == 0)
			i += size;
	}
	return {};
}

struct TLV
{
	quint32 tag {};
	quint32 length {};
	QByteArrayView data;

	constexpr TLV(QByteArrayView _data)
	{
		if (_data.isEmpty())
			return;

		auto begin = _data.cbegin();
		tag = quint8(*begin++);
		if((tag & 0x1F) == 0x1F) { // Multi-byte tag
			constexpr quint8 MAX_TAG_BYTES = sizeof(tag);
			quint8 tag_bytes = 1;
			do {
				if(tag_bytes >= MAX_TAG_BYTES || begin == _data.cend())
					return;
				tag = (tag << 8) | quint8(*begin++);
				++tag_bytes;
			} while ((tag & 0x80) != 0x00);
		}

		if (begin == _data.cend())
			return;

		length = quint8(*begin++);
		if (length & 0x80) { // Extended length encoding
			constexpr quint8 MAX_LEN_BYTES = sizeof(length);
			auto len_bytes = quint8(length & 0x7F);
			if (len_bytes == 0 || len_bytes > MAX_LEN_BYTES
				|| std::distance(begin, _data.cbegin()) < len_bytes) {
				return;
			}

			length = 0;
			for (quint8 i = 0; i < len_bytes; ++i) {
				length = (length << 8) | quint8(*begin++);
			}
		}

		if (std::distance(begin, _data.cend()) >= length) {
			data = QByteArrayView(begin, _data.cend());
		}
	}

	TLV operator[](quint32 find) const
	{
		return TLV({data.cbegin(), data.cbegin() + length}).find(find);
	}
	TLV& operator++() { return *this = TLV({data.cbegin() + length, data.cend()}); }

	TLV find(quint32 find) const
	{
		TLV tlv = *this;
		for (; tlv && tlv.tag != find; ++tlv) {}
		// Return the found TLV or an empty one if not found
		return tlv;
	}

	operator bool() const { return !data.isEmpty(); }
};



const QByteArray IDEMIACard::AID = APDU("00A4040C 10 A000000077010800070000FE00000100");
const QByteArray IDEMIACard::AID_OT = APDU("00A4040C 0D E828BD080FF2504F5420415750");
const QByteArray IDEMIACard::AID_QSCD = APDU("00A4040C 10 51534344204170706C69636174696F6E");
const QByteArray IDEMIACard::ATR_COSMO8 = QByteArrayLiteral("3BDB960080B1FE451F830012233F536549440F9000F1");
const QByteArray IDEMIACard::ATR_COSMOX = QByteArrayLiteral("3BDC960080B1FE451F830012233F54654944320F9000C3");

QPCSCReader::Result IDEMIACard::change(QPCSCReader *reader, QSmartCardData::PinType type, const QString &pin_, const QString &newpin_) const
{
	QByteArray cmd = CHANGE;
	QByteArray newpin = pinTemplate(newpin_);
	QByteArray pin = pinTemplate(pin_);
	switch (type) {
	case QSmartCardData::Pin1Type:
		cmd[3] = 1;
		break;
	case QSmartCardData::PukType:
		cmd[3] = 2;
		break;
	case QSmartCardData::Pin2Type:
		if(auto result = reader->transfer(AID_QSCD); !result)
			return result;
		cmd[3] = char(0x85);
		break;
	}
	cmd[4] = char(pin.size() + newpin.size());
	return transfer(reader, false, cmd + pin + newpin, type, quint8(pin.size()), true);
}

bool IDEMIACard::isSupported(const QByteArray &atr)
{
	return atr == ATR_COSMO8 || atr == ATR_COSMOX;
}

bool IDEMIACard::loadPerso(QPCSCReader *reader, QSmartCardDataPrivate *d) const
{
	if(d->data.isEmpty() && reader->transfer(APDU("00A4090C 04 3F00 5000")))
	{
		QByteArray cmd = APDU("00A4020C 02 5001");
		using enum QSmartCardData::PersonalDataType;
		for(char data = SurName; data <= Expiry; ++data)
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
			case SurName:
			case FirstName:
			case Citizen:
			case Id:
			case DocumentId:
				d->data[QSmartCardData::PersonalDataType(data)] = record;
				break;
			case BirthDate:
				if(!record.isEmpty())
					d->data[BirthDate] = QDate::fromString(record.left(10), QStringLiteral("dd MM yyyy"));
				break;
			case Expiry:
				d->data[Expiry] = QDateTime::fromString(record, QStringLiteral("dd MM yyyy")).addDays(1).addSecs(-1);
				break;
			default: break;
			}
		}
	}

	auto readCert = [&](const QByteArray &path) {
		QPCSCReader::Result data = reader->transfer(path);
		if(!data)
			return QSslCertificate();
		auto sizeTag = parseFCI(data.data, 0x80);
		if(sizeTag.isEmpty())
			return QSslCertificate();
		QByteArray cert;
		QByteArray cmd = READBINARY;
		qsizetype maxLe = 0;
		if(reader->atr() == ATR_COSMOX)
			maxLe = 0xC0;
		for(qsizetype size = quint8(sizeTag[0]) << 8 | quint8(sizeTag[1]); cert.size() < size; )
		{
			cmd[2] = char(cert.size() >> 8);
			cmd[3] = char(cert.size());
			cmd[4] = char(std::min(size - cert.size(), maxLe));
			data = reader->transfer(cmd);
			if(!data)
				return QSslCertificate();
			cert += data.data;
		}
		return QSslCertificate(cert, QSsl::Der);
	};
	if(d->authCert.isNull())
		d->authCert = readCert(APDU("00A40904 06 3F00 ADF1 3401 00"));
	if(d->signCert.isNull())
		d->signCert = readCert(APDU("00A40904 06 3F00 ADF2 341F 00"));
	if(d->authCert.isNull() || d->signCert.isNull())
		return false;
	if(!d->data.contains(QSmartCardData::BirthDate))
		d->data[QSmartCardData::BirthDate] = IKValidator::birthDate(d->authCert.personalCode());
	return updateCounters(reader, d);
}

QByteArray IDEMIACard::pinTemplate(const QString &pin)
{
	QByteArray result = pin.toUtf8();
#if QT_VERSION >= QT_VERSION_CHECK(6, 4, 0)
	result.resize(12, char(0xFF));
#else
	result += QByteArray(12 - result.size(), char(0xFF));
#endif
	return result;
}

QPCSCReader::Result IDEMIACard::replace(QPCSCReader *reader, QSmartCardData::PinType type, const QString &puk_, const QString &pin_) const
{
	QByteArray puk = pinTemplate(puk_);
	QByteArray cmd = VERIFY;
	cmd[3] = 2;
	cmd[4] = char(puk.size());
	if(auto result = transfer(reader, true, cmd + puk, QSmartCardData::PukType, 0, true); !result)
		return result;

	cmd = Card::REPLACE;
	cmd[2] = 2;
	if(type == QSmartCardData::Pin2Type)
	{
		if(auto result = reader->transfer(IDEMIACard::AID_QSCD); !result)
			return result;
		cmd[3] = char(0x85);
	}
	else
		cmd[3] = char(type);
	QByteArray pin = pinTemplate(pin_);
	cmd[4] = char(pin.size());
	return transfer(reader, false, cmd + pin, type, 0, false);
}

bool IDEMIACard::updateCounters(QPCSCReader *reader, QSmartCardDataPrivate *d) const
{
	reader->transfer(AID);
	if(auto data = reader->transfer(APDU("00CB3FFF 0A 4D087006 BF8101 02A080 00")))
	{
		if(auto info = TLV(data.data)[0xBF8101][0xA0]; auto retry = info[0x9B])
			d->retry[QSmartCardData::Pin1Type] = quint8(retry.data[0]);
	}
	if(auto data = reader->transfer(APDU("00CB3FFF 0A 4D087006 BF8102 02A080 00")))
	{
		if(auto info = TLV(data.data)[0xBF8102][0xA0]; auto retry = info[0x9B])
			d->retry[QSmartCardData::PukType] = quint8(retry.data[0]);
	}
	reader->transfer(AID_QSCD);
	if(auto data = reader->transfer(APDU("00CB3FFF 0A 4D087006 BF8105 02A080 00")))
	{
		if(auto info = TLV(data.data)[0xBF8105][0xA0]; auto retry = info[0x9B])
			d->retry[QSmartCardData::Pin2Type] = quint8(retry.data[0]);
	}
	return true;
}



const QByteArray THALESCard::AID = APDU("00A4040C 0C A000000063504B43532D3135");

QPCSCReader::Result THALESCard::change(QPCSCReader *reader, QSmartCardData::PinType type, const QString &pin_, const QString &newpin_) const
{
	QByteArray cmd = CHANGE;
	QByteArray newpin = pinTemplate(newpin_);
	QByteArray pin = pinTemplate(pin_);
	cmd[3] = char(0x80 | type);
	cmd[4] = char(pin.size() + newpin.size());
	return transfer(reader, false, cmd + pin + newpin, type, quint8(pin.size()), true);
}

bool THALESCard::isSupported(const QByteArray &atr)
{
	return atr == "3BFF9600008031FE438031B85365494464B085051012233F1D";
}

bool THALESCard::loadPerso(QPCSCReader *reader, QSmartCardDataPrivate *d) const
{
	d->pukReplacable = false;
	if(d->data.isEmpty() && reader->transfer(APDU("00A4080C 02 DFDD")))
	{
		QByteArray cmd = APDU("00A4020C 02 5001");
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
			case QSmartCardData::SurName:
			case QSmartCardData::FirstName:
			case QSmartCardData::Citizen:
			case QSmartCardData::Id:
			case QSmartCardData::DocumentId:
				d->data[QSmartCardData::PersonalDataType(data)] = record;
				break;
			case QSmartCardData::BirthDate:
				if(!record.isEmpty())
					d->data[QSmartCardData::BirthDate] = QDate::fromString(record.left(10), QStringLiteral("dd MM yyyy"));
				break;
			case QSmartCardData::Expiry:
				d->data[QSmartCardData::Expiry] = QDateTime::fromString(record, QStringLiteral("dd MM yyyy")).addDays(1).addSecs(-1);
				break;
			default: break;
			}
		}
	}

	bool readFailed = false;
	auto readCert = [&](const QByteArray &path) {
		QPCSCReader::Result data = reader->transfer(path);
		if(!data)
		{
			readFailed = true;
			return QSslCertificate();
		}
		auto sizeTag = parseFCI(data.data, 0x81);
		if(sizeTag.isEmpty())
			return QSslCertificate();
		QByteArray cert;
		QByteArray cmd = READBINARY;
		for(int size = quint8(sizeTag[0]) << 8 | quint8(sizeTag[1]); cert.size() < size;)
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
		d->authCert = readCert(APDU("00A40804 04 ADF1 3411 00"));
	if(d->signCert.isNull())
		d->signCert = readCert(APDU("00A40804 04 ADF2 3421 00"));

	if(readFailed)
		return false;
	return updateCounters(reader, d);
}

QByteArray THALESCard::pinTemplate(const QString &pin)
{
	QByteArray result = pin.toUtf8();
#if QT_VERSION >= QT_VERSION_CHECK(6, 4, 0)
	result.resize(12, char(0x00));
#else
	result += QByteArray(12 - result.size(), char(0x00));
#endif
	return result;
}

QPCSCReader::Result THALESCard::replace(QPCSCReader *reader, QSmartCardData::PinType type, const QString &puk_, const QString &pin_) const
{
	QByteArray puk = pinTemplate(puk_);
	QByteArray pin = pinTemplate(pin_);
	QByteArray cmd = REPLACE;
	cmd[3] = char(0x80 | type);
	cmd[4] = char(puk.size() + pin.size());
	return transfer(reader, false, cmd + puk + pin, type, quint8(puk.size()), true);
}

bool THALESCard::updateCounters(QPCSCReader *reader, QSmartCardDataPrivate *d) const
{
	auto apdu = APDU("00CB00FF 05 A003830180 00");
	for(quint8 type = QSmartCardData::Pin1Type; type <= QSmartCardData::PukType; ++type)
	{
		apdu[9] = char(0x80 | type);
		auto data = reader->transfer(apdu);
		if(!data)
			return false;
		if(auto info = TLV(data.data); auto retry = info[0xDF21])
		{
			d->retry[QSmartCardData::PinType(type)] = quint8(retry.data[0]);
			auto changed = info[0xDF2F];
			d->locked[QSmartCardData::PinType(type)] = changed && changed.data[0] == 0;
		}
		else
			return false;
	}
	return true;
}



QSmartCard::QSmartCard(QObject *parent)
	: QObject(parent)
	, d(new Private)
{}

QSmartCard::~QSmartCard() noexcept = default;

QSmartCardData QSmartCard::data() const { return d->t; }

bool QSmartCard::pinChange(QSmartCardData::PinType type, QSmartCard::PinAction action, QWidget* parent)
{
	std::unique_ptr<PinPopup,QScopedPointerDeleteLater> popup;
	QByteArray oldPin, newPin;
	QSmartCardData::PinType src = type;
	if(action != QSmartCard::ChangeWithPin && action != QSmartCard::ActivateWithPin)
	{
		if(type == QSmartCardData::PukType)
			return false;
		src = QSmartCardData::PukType;
	}

	if(d->t.isPinpad())
	{
		QString bodyText;
		switch(action)
		{
		case QSmartCard::ActivateWithPuk:
		case QSmartCard::ChangeWithPuk:
			bodyText = tr("To change %1 code with the PUK code on a PinPad reader the PUK code has to be entered first and then the %1 code twice.").arg(QSmartCardData::typeString(type));
			break;
		case QSmartCard::UnblockWithPuk:
			bodyText = tr("To unblock the %1 code on a PinPad reader the PUK code has to be entered first and then the %1 code twice.").arg(QSmartCardData::typeString(type));
			break;
		default:
			bodyText = tr("To change %1 on a PinPad reader the old %1 code has to be entered first and then the new %1 code twice.").arg(QSmartCardData::typeString(type));
			break;
		}

		PinPopup::TokenFlags flags = PinPopup::PinpadChangeFlag;
		switch(d->t.retryCount(src))
		{
		case 2: flags |= PinPopup::PinCountLow; break;
		case 1: flags |= PinPopup::PinFinalTry; break;
		case 0: flags |= PinPopup::PinLocked; break;
		default: break;
		}
		popup.reset(new PinPopup(src, flags, d->t.authCert(), parent, bodyText));
		popup->open();
	}
	else
	{
		PinUnblock p(type, action, d->t.retryCount(src), d->t.data(QSmartCardData::BirthDate).toDate(),
			d->t.data(QSmartCardData::Id).toString(), d->t.isPUKReplacable(), parent);
		if (!p.exec())
			return false;
		oldPin = p.firstCodeText().toUtf8();
		newPin = p.newCodeText().toUtf8();
	}

	QPCSCReader reader(d->t.reader(), &QPCSC::instance());
	if(!reader.connect())
	{
		FadeInNotification::warning(parent, tr("Changing %1 failed").arg(QSmartCardData::typeString(type)));
		return false;
	}
	QPCSCReader::Result	response = src == QSmartCardData::PukType ?
		d->card->replace(&reader, type, oldPin, newPin) :
		d->card->change(&reader, type, oldPin, newPin);
	switch(response.SW)
	{
	case 0x9000:
		d->card->updateCounters(&reader, d->t.d);
		switch(action)
		{
			using enum QSmartCard::PinAction;
		case ActivateWithPin:
		case ActivateWithPuk:
			FadeInNotification::success(parent, tr("%1 changed!").arg(QSmartCardData::typeString(type)));
			return true;
		case ChangeWithPin:
		case ChangeWithPuk:
			FadeInNotification::success(parent, tr("%1 changed!").arg(QSmartCardData::typeString(type)));
			return false; // Do not update ui, there is no warning in MainWindow when PIN usage count is low
		case UnblockWithPuk:
			FadeInNotification::success(parent, tr("%1 has been changed and the certificate has been unblocked!")
				.arg(QSmartCardData::typeString(type)));
			return true;
		}
		return false;
	case 0x63C0: //pin retry count 0
	case 0x6983:
	case 0x6984:
		d->card->updateCounters(&reader, d->t.d);
		FadeInNotification::warning(parent, tr("%1 blocked").arg(QSmartCardData::typeString(src)));
		return true;
	case 0x63C1: // Validate error, 1 tries left
	case 0x63C2: // Validate error, 2 tries left
	case 0x63C3:
		d->card->updateCounters(&reader, d->t.d);
		FadeInNotification::warning(parent, tr("Wrong %1 code. You can try %n more time(s).", nullptr,
			d->t.retryCount(src)).arg(QSmartCardData::typeString(src)));
		return false;
	case 0x6400:
		FadeInNotification::warning(parent, tr("%1 timeout").arg(QSmartCardData::typeString(type)));
		return false; // Timeout (SCM)
	case 0x6401:
		return false; // Cancel (OK, SCM)
	case 0x6402:
		FadeInNotification::warning(parent, tr("New %1 codes doesn't match").arg(QSmartCardData::typeString(type)));
		return false;
	case 0x6403:
		FadeInNotification::warning(parent, tr("%1 length has to be between %2 and 12")
			.arg(QSmartCardData::typeString(src)).arg(QSmartCardData::minPinLen(src)));
		return false;
	case 0x6985:
	case 0x6A80:
		FadeInNotification::warning(parent, tr("Old and new %1 has to be different!").arg(QSmartCardData::typeString(type)));
		return false;
	default:
		FadeInNotification::warning(parent, tr("Changing %1 failed").arg(QSmartCardData::typeString(type)));
		return false;
	}
}

void QSmartCard::reloadCard(const TokenData &token, bool reloadCounters)
{
	qCDebug(CLog) << "Polling";
	d->token = token;
	if(!reloadCounters && !d->t.isNull() && !d->t.card().isEmpty() && d->t.card() == d->token.card())
		return;

	std::thread([&] {
		// check if selected card is same as signer
		if(!d->t.card().isEmpty() && d->token.card() != d->t.card())
			d->t.d = new QSmartCardDataPrivate();

		// select signer card
		if(d->t.card().isEmpty() || d->t.card() != d->token.card())
		{
			QSharedDataPointer<QSmartCardDataPrivate> t = d->t.d;
			t->card = d->token.card();
			t->data.clear();
			t->authCert.clear();
			t->signCert.clear();
			d->t.d = std::move(t);
		}

		if(!reloadCounters && (!d->t.isNull() || d->token.reader().isEmpty()))
			return;

		QString reader = d->token.reader();
		if(d->token.reader().endsWith(QLatin1String("..."))) {
			for(const QString &test: QPCSC::instance().readers()) {
				if(test.startsWith(d->token.reader().left(d->token.reader().size() - 3)))
					reader = test;
			}
		}

		qCDebug(CLog) << "Read" << reader;
		QPCSCReader selectedReader(reader, &QPCSC::instance());
		if(!selectedReader.connect())
			return;

		if(auto atr = selectedReader.atr();
			IDEMIACard::isSupported(atr))
			d->card = std::make_unique<IDEMIACard>();
		else if(THALESCard::isSupported(atr))
			d->card = std::make_unique<THALESCard>();
		else {
			qCDebug(CLog) << "Unsupported card";
			return;
		}

		qCDebug(CLog) << "Read card" << d->token.card() << "info";
		QSharedDataPointer<QSmartCardDataPrivate> t = d->t.d;
		t->reader = selectedReader.name();
		t->pinpad = selectedReader.isPinPad();
		if(d->card->loadPerso(&selectedReader, t))
		{
			d->t.d = std::move(t);
			emit dataChanged(d->t);
		}
		else
			qCDebug(CLog) << "Failed to read card info, try again next round";
	}).detach();
}

TokenData QSmartCard::tokenData() const { return d->token; }

