/*
 * QEstEidCommon
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

#include "QPCSC_p.h"

#include <QtCore/QDateTime>
#include <QtCore/QLoggingCategory>
#include <QtCore/QStringList>
#include <QtCore/QtEndian>

#include <array>
#include <cstring>

static Q_LOGGING_CATEGORY(APDU,"QPCSC.APDU")
static Q_LOGGING_CATEGORY(SCard,"QPCSC.SCard")

static quint16 toUInt16(const QByteArray &data, int size)
{
	return size >= 2 ? quint16((quint16(data[size - 2]) << 8) | quint8(data[size - 1])) : 0;
}

static QStringList stateToString(DWORD state)
{
	QStringList result;
	#define STATE(X) if(state & SCARD_STATE_##X) result.append(QStringLiteral(#X))
	STATE(IGNORE);
	STATE(CHANGED);
	STATE(UNKNOWN);
	STATE(UNAVAILABLE);
	STATE(EMPTY);
	STATE(PRESENT);
	STATE(ATRMATCH);
	STATE(EXCLUSIVE);
	STATE(INUSE);
	STATE(MUTE);
	return result;
}

template<auto Func, typename... Args>
static auto SCCall(const char *file, int line, const char *function, Args&& ...args)
{
	auto err = Func(std::forward<Args>(args)...);
	if(SCard().isDebugEnabled())
		QMessageLogger(file, line, function, SCard().categoryName()).debug()
			<< function << Qt::hex << (unsigned long)err;
	return err;
}
#define SC(API, ...) SCCall<SCard##API>(__FILE__, __LINE__, "SCard"#API, __VA_ARGS__)

QHash<DRIVER_FEATURES,quint32> QPCSCReader::Private::features()
{
	if(!featuresList.isEmpty())
		return featuresList;
	DWORD size = 0;
	std::array<PCSC_TLV_STRUCTURE,FEATURE_CCID_ESC_COMMAND> feature{};
	if(SC(Control, card, DWORD(CM_IOCTL_GET_FEATURE_REQUEST), nullptr, 0U, feature.data(), DWORD(feature.size() * sizeof(PCSC_TLV_STRUCTURE)), &size))
		return featuresList;
	if(size % sizeof(PCSC_TLV_STRUCTURE))
		return featuresList;
	for(const auto &f: feature)
	{
		if(f.tag)
			featuresList[f.tag] = qFromBigEndian<quint32>(f.value);
	}
	return featuresList;
}



QPCSC::QPCSC()
	: d(new Private)
{
	const_cast<QLoggingCategory&>(SCard()).setEnabled(QtDebugMsg, qEnvironmentVariableIsSet("PCSC_DEBUG"));
	const_cast<QLoggingCategory&>(APDU()).setEnabled(QtDebugMsg, qEnvironmentVariableIsSet("APDU_DEBUG"));
	Q_UNUSED(serviceRunning())
}

QPCSC::~QPCSC()
{
	requestInterruption();
	d->sleepCond.wakeAll();
	if(d->thread)
		SC(Cancel, d->thread);
	wait();
	if(d->context)
		SC(ReleaseContext, d->context);
	delete d;
}

QPCSC& QPCSC::instance()
{
	static QPCSC pcsc;
	return pcsc;
}

QByteArray QPCSC::rawReaders() const
{
	if( !serviceRunning() )
		return {};

	DWORD size = 0;
	LONG err = SC(ListReaders, d->context, nullptr, nullptr, &size);
	if(err != SCARD_S_SUCCESS || !size)
		return {};

	QByteArray data(int(size), 0);
	err = SC(ListReaders, d->context, nullptr, data.data(), &size);
	if(err != SCARD_S_SUCCESS)
		data.clear();
	return data;
}

QStringList QPCSC::readers() const
{
	QByteArray tmp = rawReaders();
	QStringList readers = QString::fromLocal8Bit(tmp.data(), tmp.size()).split(QChar(0));
	readers.removeAll({});
	return readers;
}

void QPCSC::run()
{
	QPCSC pcsc;
	std::vector<SCARD_READERSTATE> list;
	while(!isInterruptionRequested())
	{
		// "\\?PnP?\Notification" does not work on macOS
		QByteArray data = pcsc.rawReaders();
		if(data.isEmpty())
		{
			QMutexLocker locker(&d->sleepMutex);
			if (isInterruptionRequested())
				break;
			d->sleepCond.wait(&d->sleepMutex, 5000);
			continue;
		}
		for(const char *name = data.constData(); *name; name += strlen(name) + 1)
		{
			if(std::none_of(list.cbegin(), list.cend(), [&name](const SCARD_READERSTATE &state) { return strcmp(state.szReader, name) == 0; }))
				list.push_back({ strdup(name), nullptr, 0, 0, 0, {} });
		}
		if(list.empty())
		{
			QMutexLocker locker(&d->sleepMutex);
			if (isInterruptionRequested())
				break;
			d->sleepCond.wait(&d->sleepMutex, 5000);
			continue;
		}
		d->thread = pcsc.d->context;
		if(SC(GetStatusChange, pcsc.d->context, 5*1000U, list.data(), DWORD(list.size())) != SCARD_S_SUCCESS)
			continue;
		for(auto i = list.begin(); i != list.end(); )
		{
			if((i->dwEventState & SCARD_STATE_CHANGED) == 0)
			{
				++i;
				continue;
			}
			if((i->dwCurrentState & SCARD_STATE_PRESENT) != (i->dwEventState & SCARD_STATE_PRESENT))
				Q_EMIT cardChanged();
			i->dwCurrentState = i->dwEventState;
			qCDebug(SCard) << "New state: " << QString::fromLocal8Bit(i->szReader) << stateToString(i->dwCurrentState);
			Q_EMIT statusChanged(QString::fromLocal8Bit(i->szReader), stateToString(i->dwCurrentState));
			if((i->dwCurrentState & (SCARD_STATE_UNKNOWN|SCARD_STATE_IGNORE)) > 0)
			{
				free((void*)i->szReader);
				i = list.erase(i);
			}
			else
				++i;
		}
	}
	d->thread = {};
}

bool QPCSC::serviceRunning() const
{
	if(d->context && SC(IsValidContext, d->context) == SCARD_S_SUCCESS)
		return true;
	SC(EstablishContext, DWORD(SCARD_SCOPE_USER), nullptr, nullptr, &d->context);
	return d->context;
}



QPCSCReader::QPCSCReader( const QString &reader, QPCSC *parent )
	: d(new Private)
{
	d->d = parent->d;
	d->reader = reader.toUtf8();
	d->state.szReader = d->reader.constData();
	if(parent->d->context)
		SC(GetStatusChange, d->d->context, 0, &d->state, 1U);
}

QPCSCReader::QPCSCReader(QPCSCReader &&other) noexcept
	: d(other.d)
{
	other.d = new Private;
}

QPCSCReader::~QPCSCReader() noexcept
{
	if(d->isTransacted)
		SC(EndTransaction, d->card, SCARD_LEAVE_CARD);
	if(d->card)
		SC(Disconnect, d->card, SCARD_LEAVE_CARD);
	delete d;
}

QByteArray QPCSCReader::atr() const
{
	return QByteArray::fromRawData((const char*)d->state.rgbAtr, int(d->state.cbAtr)).toHex().toUpper();
}

bool QPCSCReader::connect(Connect connect, Mode mode)
{
	if(SC(Connect, d->d->context, d->state.szReader, connect, mode, &d->card, &d->io.dwProtocol) != SCARD_S_SUCCESS)
		return false;
	d->isTransacted = SC(BeginTransaction, d->card) == SCARD_S_SUCCESS;
	return true;
}

bool QPCSCReader::isPinPad() const
{
	if(d->reader.contains("HID Global OMNIKEY 3x21 Smart Card Reader") ||
		d->reader.contains("HID Global OMNIKEY 6121 Smart Card Reader"))
		return false;
	if(qEnvironmentVariableIsSet("SMARTCARDPP_NOPINPAD"))
		return false;
	QHash<DRIVER_FEATURES,quint32> features = d->features();
	return features.contains(FEATURE_VERIFY_PIN_DIRECT) || features.contains(FEATURE_VERIFY_PIN_START);
}

bool QPCSCReader::isPresent() const
{
	return d->state.dwEventState & SCARD_STATE_PRESENT;
}

QString QPCSCReader::name() const
{
	return QString::fromLocal8Bit( d->reader );
}

QHash<QPCSCReader::Properties, int> QPCSCReader::properties() const
{
	QHash<Properties,int> properties;
	if( DWORD ioctl = d->features().value(FEATURE_GET_TLV_PROPERTIES) )
	{
		DWORD size = 0;
		std::array<BYTE,256> recv{};
		if(SC(Control, d->card, ioctl, nullptr, 0U, recv.data(), DWORD(recv.size()), &size))
			return properties;
		for(auto p = recv.cbegin(); std::distance(recv.cbegin(), p) < size; )
		{
			int tag = *p++, len = *p++, value = 0;
			for(int i = 0; i < len; ++i)
				value |= *p++ << 8 * i;
			properties.emplace(Properties(tag), value);
		}
	}
	return properties;
}

QStringList QPCSCReader::state() const
{
	return stateToString(d->state.dwEventState);
}

QPCSCReader::Result QPCSCReader::transfer( const QByteArray &apdu ) const
{
	QByteArray data( 1024, 0 );
	auto size = DWORD(data.size());

	qCDebug(APDU).nospace().noquote() << 'T' << d->io.dwProtocol - 1 << "> " << apdu.toHex();
	LONG ret = SC(Transmit, d->card, &d->io,
		LPCBYTE(apdu.constData()), DWORD(apdu.size()), nullptr, LPBYTE(data.data()), &size);
	if( ret != SCARD_S_SUCCESS )
		return { {}, {}, quint32(ret) };

	Result result { data.left(int(size - 2)), toUInt16(data, size), quint32(ret) };
	qCDebug(APDU).nospace() << 'T' << d->io.dwProtocol - 1 << "< " << Qt::hex << result.SW;
	if(!result.data.isEmpty()) qCDebug(APDU).nospace().noquote() << result.data.toHex();

	switch(result.SW & 0xFF00)
	{
	case 0x6100: // Read more
	{
		QByteArray cmd( "\x00\xC0\x00\x00\x00", 5 );
		cmd[4] = char(result.SW);
		Result result2 = transfer( cmd );
		result2.data.prepend(result.data);
		return result2;
	}
	case 0x6C00: // Excpected lenght
	{
		QByteArray cmd = apdu;
		cmd[4] = char(result.SW);
		return transfer(cmd);
	}
	default: return result;
	}
}

QPCSCReader::Result QPCSCReader::transferCTL(const QByteArray &apdu, bool verify,
	quint16 lang, quint8 minlen, quint8 newPINOffset, bool requestCurrentPIN) const
{
	bool display = false;
	QHash<DRIVER_FEATURES,quint32> features = d->features();
	if( DWORD ioctl = features.value(FEATURE_IFD_PIN_PROPERTIES) )
	{
		PIN_PROPERTIES_STRUCTURE caps{};
		DWORD size = sizeof(caps);
		if(!SC(Control, d->card, ioctl, nullptr, 0U, &caps, size, &size))
			display = caps.wLcdLayout > 0;
	}

	quint8 PINFrameOffset = 0, PINLengthOffset = 0;
	auto toByteArray = [&](auto &data) {
		data.bTimerOut = 30;
		data.bTimerOut2 = 30;
		data.bmFormatString = FormatASCII|AlignLeft|quint8(PINFrameOffset << 4)|PINFrameOffsetUnitBits;
		data.bmPINBlockString = PINLengthNone << 5|PINFrameSizeAuto;
		data.bmPINLengthFormat = PINLengthOffsetUnitBits|PINLengthOffset;
		data.wPINMaxExtraDigit = quint16(minlen << 8) | 12;
		data.bEntryValidationCondition = ValidOnKeyPressed;
		data.wLangId = lang;
		data.ulDataLength = quint32(apdu.size());
		return QByteArray((const char*)&data, sizeof(data) - 1) + apdu;
	};

	QByteArray cmd;
	if( verify )
	{
		PIN_VERIFY_STRUCTURE data{};
		data.bNumberMessage = display ? CCIDDefaultInvitationMessage : NoInvitationMessage;
		data.bMsgIndex = NoInvitationMessage;
		cmd = toByteArray(data);
	}
	else
	{
		PIN_MODIFY_STRUCTURE data{};
		data.bNumberMessage = display ? ThreeInvitationMessage : NoInvitationMessage;
		data.bInsertionOffsetOld = 0x00;
		data.bInsertionOffsetNew = newPINOffset;
		data.bConfirmPIN = ConfirmNewPin;
		if(requestCurrentPIN)
		{
			data.bConfirmPIN |= RequestCurrentPin;
			data.bMsgIndex1 = NoInvitationMessage;
			data.bMsgIndex2 = OneInvitationMessage;
			data.bMsgIndex3 = TwoInvitationMessage;
		}
		else
		{
			data.bMsgIndex1 = OneInvitationMessage;
			data.bMsgIndex2 = TwoInvitationMessage;
			data.bMsgIndex3 = ThreeInvitationMessage;
		}
		cmd = toByteArray(data);
	}

	DWORD ioctl = features.value( verify ? FEATURE_VERIFY_PIN_START : FEATURE_MODIFY_PIN_START );
	if( !ioctl )
		ioctl = features.value( verify ? FEATURE_VERIFY_PIN_DIRECT : FEATURE_MODIFY_PIN_DIRECT );

	qCDebug(APDU).nospace().noquote() << 'T' << d->io.dwProtocol - 1 << "> " << apdu.toHex();
	qCDebug(APDU).nospace().noquote() << "CTL" << "> " << cmd.toHex();
	QByteArray data( 255 + 3, 0 );
	auto size = DWORD(data.size());
	LONG err = SC(Control, d->card, ioctl, cmd.constData(), DWORD(cmd.size()), LPVOID(data.data()), DWORD(data.size()), &size);

	if( DWORD finish = features.value( verify ? FEATURE_VERIFY_PIN_FINISH : FEATURE_MODIFY_PIN_FINISH ) )
	{
		size = DWORD(data.size());
		err = SC(Control, d->card, finish, nullptr, 0U, LPVOID(data.data()), DWORD(data.size()), &size);
	}

	Result result { data.left(int(size - 2)), toUInt16(data, size), quint32(err) };
	qCDebug(APDU).nospace() << 'T' << d->io.dwProtocol - 1 << "< " << Qt::hex << result.SW;
	if(!result.data.isEmpty()) qCDebug(APDU).nospace().noquote() << result.data.toHex();
	return result;
}
