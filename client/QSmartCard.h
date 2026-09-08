// SPDX-FileCopyrightText: Estonian Information System Authority
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include <QObject>
#include <QSharedDataPointer>

#include <memory>

class SslCertificate;
class QSmartCardDataPrivate;
class QSslKey;
class TokenData;

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

class QSmartCard final: public QObject
{
	Q_OBJECT
public:
	enum PinAction : quint8
	{
		ActivateWithPuk,
		ActivateWithPin,
		ChangeWithPin,
		ChangeWithPuk,
		UnblockWithPuk,
	};

	explicit QSmartCard(QObject *parent = nullptr);
	~QSmartCard() noexcept final;

	QSmartCardData data() const;
	TokenData tokenData() const;
	void reloadCard(const TokenData &token, bool reloadCounters);

	bool pinChange(QSmartCardData::PinType type, PinAction action = ChangeWithPin, QWidget* parent = nullptr);

Q_SIGNALS:
	void tokenChanged(const TokenData &data);
	void dataChanged(const QSmartCardData &data);

private:
	Q_DISABLE_COPY_MOVE(QSmartCard)

	struct Private;
	std::unique_ptr<Private> d;
};
