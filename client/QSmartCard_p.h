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

#include "QSmartCard.h"

#include <common/QPCSC.h>
#include <common/SslCertificate.h>

#include <QtCore/QStringList>
#include <QtCore/QTextCodec>
#include <QtCore/QVariant>

#define APDU QByteArray::fromHex

class QSmartCardPrivate
{
public:
	QSharedPointer<QPCSCReader> connect(const QString &reader);
	QSmartCard::ErrorType handlePinResult(QPCSCReader *reader, const QPCSCReader::Result &response, bool forceUpdate);
	quint16 language() const;
	static QHash<quint8,QByteArray> parseFCI(const QByteArray &data);
	bool updateCounters(QPCSCReader *reader, QSmartCardDataPrivate *d);

	QSharedPointer<QPCSCReader> reader;
	QSmartCardData	t;
	QTextCodec		*codec = QTextCodec::codecForName("Windows-1252");

	const QByteArray AID30 = APDU("00A40400 10 D2330000010000010000000000000000");
	const QByteArray AID34 = APDU("00A40400 0E F04573744549442076657220312E");
	const QByteArray AID35 = APDU("00A40400 0F D23300000045737445494420763335");
	const QByteArray UPDATER_AID =	APDU("00A40400 0A D2330000005550443101");
	const QByteArray MASTER_FILE =	APDU("00A4000C");// 00"); // Compatibilty for some cards
	const QByteArray ESTEIDDF =		APDU("00A4010C 02 EEEE");
	const QByteArray PERSONALDATA =	APDU("00A4020C 02 5044");
	const QByteArray AUTHCERT =		APDU("00A40200 02 AACE");
	const QByteArray SIGNCERT =		APDU("00A40200 02 DDCE");
	const QByteArray KEYPOINTER =	APDU("00A4020C 02 0033");
	const QByteArray KEYUSAGE =		APDU("00A4020C 02 0013");
	const QByteArray PINRETRY =		APDU("00A4020C 02 0016");
	const QByteArray READBINARY =	APDU("00B00000 00");
	const QByteArray READRECORD =	APDU("00B20004 00");
	const QByteArray SECENV1 =		APDU("0022F301");// 00"); // Compatibilty for some cards
	const QByteArray SECENV3 =		APDU("0022F303 00");
	const QByteArray CHANGE =		APDU("00240000 00");
	const QByteArray REPLACE =		APDU("002C0000 00");
	const QByteArray VERIFY =		APDU("00200000 00");
	const QByteArray APPLETVER =	APDU("00CA0100 00");
};

class QSmartCardDataPrivate: public QSharedData
{
public:
	static const QHash<QByteArray,QSmartCardData::CardVersion> atrList;

	QString card, reader, appletVersion;
	QHash<QSmartCardData::PersonalDataType,QVariant> data;
	SslCertificate authCert, signCert;
	QHash<QSmartCardData::PinType,quint8> retry;
	QHash<QSmartCardData::PinType,ulong> usage;
	QSmartCardData::CardVersion version = QSmartCardData::VER_INVALID;
	bool pinpad = false;
};
