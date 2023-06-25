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

#include <QObject>
#include <QSharedDataPointer>

class SslCertificate;
class QSmartCardDataPrivate;
class QSslKey;
class TokenData;

class QSmartCardData
{
public:
	enum PersonalDataType
	{
		SurName = 1,
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
		VER_3_5,
		VER_IDEMIA,
	};

	QSmartCardData();
	QSmartCardData( const QSmartCardData &other );
	QSmartCardData(QSmartCardData &&other) Q_DECL_NOEXCEPT;
	~QSmartCardData();
	QSmartCardData& operator=( const QSmartCardData &other );
	QSmartCardData& operator=(QSmartCardData &&other) Q_DECL_NOEXCEPT;
	bool operator ==(const QSmartCardData &other) const;
	bool operator !=(const QSmartCardData &other) const;

	QString card() const;
	QString reader() const;

	bool isNull() const;
	bool isPinpad() const;
	bool isValid() const;

	QVariant data( PersonalDataType type ) const;
	SslCertificate authCert() const;
	SslCertificate signCert() const;
	quint8 retryCount( PinType type ) const;
	ulong usageCount( PinType type ) const;
	CardVersion version() const;

	static quint8 minPinLen(QSmartCardData::PinType type);
	static QString typeString( PinType type );

private:
	QSharedDataPointer<QSmartCardDataPrivate> d;

	friend class QSmartCard;
	friend class QSmartCardPrivate;
};

class QSmartCard: public QObject
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

	explicit QSmartCard(QObject *parent = nullptr);
	~QSmartCard();

	ErrorType change( QSmartCardData::PinType type, QWidget* parent, const QString &newpin, const QString &pin, const QString &title, const QString &bodyText );
	QSmartCardData data() const;
	TokenData tokenData() const;
	void reloadCard(const TokenData &token);
	ErrorType unblock( QSmartCardData::PinType type, QWidget* parent, const QString &pin, const QString &puk, const QString &title, const QString &bodyText );

	ErrorType pinUnblock( QSmartCardData::PinType type, bool isForgotPin = false, QWidget* parent = nullptr );
	ErrorType pinChange( QSmartCardData::PinType type, QWidget* parent = nullptr );

	static QHash<quint8,QByteArray> parseFCI(const QByteArray &data);

signals:
	void dataChanged(const QSmartCardData &data);

private:
	void reload();

	class Private;
	Private *d;
	friend class QSigner;
};
