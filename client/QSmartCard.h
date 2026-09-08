// SPDX-FileCopyrightText: Estonian Information System Authority
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include <QCoreApplication>
#include <QSharedDataPointer>

#include <memory>

class SslCertificate;
class QSmartCardDataPrivate;
class QSslKey;
class TokenData;
class QWidget;

class QSmartCardData
{
public:
	enum PersonalDataType : char
	{
		SurName = 1,
		FirstName = 2,
		Citizen = 4,
		BirthDate = 5,
		Id = 6,
		DocumentId = 7,
		Expiry = 8,
	};
	enum PinType : quint8
	{
		Pin1Type = 1,
		Pin2Type,
		PukType
	};

	QSmartCardData();
	QSmartCardData( const QSmartCardData &other );
	QSmartCardData(QSmartCardData &&other) noexcept;
	~QSmartCardData() noexcept;
	QSmartCardData& operator=( const QSmartCardData &other );
	QSmartCardData& operator=(QSmartCardData &&other) noexcept;
	bool operator ==(const QSmartCardData &other) const;

	QString card() const;
	QString reader() const;

	bool isNull() const;
	bool isPinpad() const;
	bool isPUKReplacable() const;
	bool isValid() const;

	QVariant data( PersonalDataType type ) const;
	SslCertificate authCert() const;
	SslCertificate signCert() const;
	quint8 retryCount( PinType type ) const;
	bool pinLocked(PinType type) const;

	static quint8 minPinLen(QSmartCardData::PinType type);
	static QString typeString( PinType type );

private:
	QSharedDataPointer<QSmartCardDataPrivate> d;

	friend class QSmartCard;
	friend class QSmartCardPrivate;
};

class QSmartCard final
{
	Q_DECLARE_TR_FUNCTIONS(QSmartCard)
public:
	enum PinAction : quint8
	{
		ActivateWithPuk,
		ActivateWithPin,
		ChangeWithPin,
		ChangeWithPuk,
		UnblockWithPuk,
	};

	static bool pinChange(TokenData token, QSmartCardData current, QSmartCardData::PinType type,
		PinAction action = ChangeWithPin, QWidget* parent = nullptr);
	static QSmartCardData readCard(const TokenData &token, bool countersOnly = false);
	static QSmartCardData readCounters(QSmartCardData data);

private:
	QSmartCard() = delete;
};
