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

#pragma once

#include <QThread>

#include <QSharedDataPointer>

template<class Key, class T> class QHash;
class SslCertificate;
class QSmartCardDataPrivate;

class QSmartCardData
{
public:
	enum PersonalDataType
	{
		SurName = 0,
		FirstName1,
		FirstName2,
		Sex,
		Citizen,
		BirthDate,
		Id,
		DocumentId,
		Expiry,
		BirthPlace,
		IssueDate,
		ResidencePermit,
		Comment1,
		Comment2,
		Comment3,
		Comment4,
		Email
	};
	enum PinType
	{
		Pin1Type = 1,
		Pin2Type,
		PukType
	};
	enum CardVersion
	{
		VER_INVALID = -1,
		VER_1_0 = 0,
		VER_1_0_2007,
		VER_1_1,
		VER_3_0,
		VER_3_4,
		VER_3_0_UPPED_TO_3_4,
		VER_3_5,
		VER_USABLEUPDATER,
		VER_HASUPDATER = 128
	};

	QSmartCardData();
	QSmartCardData( const QSmartCardData &other );
	QSmartCardData( QSmartCardData &&other );
	~QSmartCardData();
	QSmartCardData& operator=( const QSmartCardData &other );
	QSmartCardData& operator=( QSmartCardData &&other );

	QString card() const;
	QStringList cards() const;
	QString reader() const;
	QStringList readers() const;

	bool isNull() const;
	bool isPinpad() const;
	bool isSecurePinpad() const;
	bool isValid() const;

	QVariant data( PersonalDataType type ) const;
	SslCertificate authCert() const;
	SslCertificate signCert() const;
	quint8 retryCount( PinType type ) const;
	ulong usageCount( PinType type ) const;
	CardVersion version() const;

	static QString typeString( PinType type );

private:
	QSharedDataPointer<QSmartCardDataPrivate> d;

	friend class QSmartCard;
	friend class QSmartCardPrivate;
};



class QSmartCardPrivate;
class QSmartCard: public QThread
{
	Q_OBJECT
public:
	enum ErrorType
	{
		NoError,
		UnknownError,
		BlockedError,
		CancelError,
		DifferentError,
		LenghtError,
		ValidateError,
		OldNewPinSameError
	};

	explicit QSmartCard( QObject *parent = 0 );
	~QSmartCard();

	ErrorType change( QSmartCardData::PinType type, const QString &newpin, const QString &pin );
	QSmartCardData data() const;
	Qt::HANDLE key();
	ErrorType login( QSmartCardData::PinType type );
	void logout();
	void reload();
	ErrorType unblock( QSmartCardData::PinType type, const QString &pin, const QString &puk );

signals:
	void dataChanged();

private Q_SLOTS:
	void selectCard( const QString &card );

private:
	void run();

	QSmartCardPrivate *d;

	friend class MainWindow;
};
