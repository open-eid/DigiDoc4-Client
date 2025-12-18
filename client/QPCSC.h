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

#pragma once

#include <QtCore/QThread>

template<typename Key, typename T> class QHash;

class QPCSCReader;
class QPCSC final: public QThread
{
	Q_OBJECT
public:
	~QPCSC() final;

	static QPCSC& instance();
	QStringList readers() const;
	bool serviceRunning() const;

Q_SIGNALS:
	void statusChanged(const QString &reader, const QStringList &state);

private:
	QPCSC();
	Q_DISABLE_COPY(QPCSC)

	QByteArray rawReaders() const;
	void run() final;

	class Private;
	Private *d;

	friend class QPCSCReader;
};

class QPCSCReader final: public QObject
{
	Q_OBJECT
public:
	struct Result {
		QByteArray data;
		quint16 SW = 0;
		quint32 err = 0;
		constexpr operator bool() const { return SW == 0x9000; }
	};

	enum Properties {
		wLcdLayout					= 0x01,
		bEntryValidationCondition	= 0x02,
		bTimeOut2					= 0x03,
		wLcdMaxCharacters			= 0x04,
		wLcdMaxLines				= 0x05,
		bMinPINSize					= 0x06,
		bMaxPINSize					= 0x07,
		sFirmwareID					= 0x08,
		bPPDUSupport				= 0x09,
		dwMaxAPDUDataSize			= 0x0A,
		wIdVendor					= 0x0B,
		wIdProduct					= 0x0C
	};

	enum Connect {
		Exclusive = 1,
		Shared = 2,
		Direct = 3
	};

	enum Reset
	{
		LeaveCard = 0,
		ResetCard = 1,
		UnpowerCard = 2,
		EjectCard = 3
	};

	enum Mode {
		Undefined = 0,
		T0 = 1,
		T1 = 2
	};

	explicit QPCSCReader( const QString &reader, QPCSC *parent );
	~QPCSCReader() final;

	QByteArray atr() const;
	bool isPinPad() const;
	bool isPresent() const;
	QString name() const;
	QHash<Properties,int> properties() const;
	QStringList state() const;
	bool updateState( quint32 msec = 0 );

	bool connect( Connect connect = Shared, Mode mode = Mode(T0|T1) );
	quint32 connectEx( Connect connect = Shared, Mode mode = Mode(T0|T1) );
	void disconnect( Reset reset = LeaveCard );
	bool reconnect( Reset reset = LeaveCard, Mode mode = Mode(T0|T1) );
	bool beginTransaction();
	bool endTransaction( Reset reset = LeaveCard );
	Result transfer( const QByteArray &apdu ) const;
	Result transferCTL(const QByteArray &apdu, bool verify, quint16 lang = 0,
		quint8 minlen = 4, quint8 newPINOffset = 0, bool requestCurrentPIN = true) const;

private:
	Q_DISABLE_COPY(QPCSCReader)
	class Private;
	Private *d;
};
