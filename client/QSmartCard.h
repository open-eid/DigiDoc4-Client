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

	class Private;
	std::unique_ptr<Private> d;
};
