/*
 * QDigiDoc4
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

#include "TokenData.h"

#include <QtCore/QCoreApplication>
#include <QtCore/QCryptographicHash>
#include <QtCore/QThread>

#include <cstdint>
#include <expected>
#include <vector>

class QSmartCardData;

class QCryptoBackend
{
	Q_DECLARE_TR_FUNCTIONS(QCryptoBackend);
public:
	enum Status : quint8
	{
		PinOK,
		PinCanceled,
		PinIncorrect,
		PinLocked,
		InProgress,
		DeviceError,
		GeneralError,
		UnknownError
	};
	enum Padding : quint8
	{
		NoPadding,
		PKCS1Padding,
		PSSPadding,
	};

	virtual ~QCryptoBackend();

	virtual std::vector<uint8_t> decrypt(QByteArrayView data, bool oaep) const = 0;
	virtual std::vector<uint8_t> deriveConcatKDF(QByteArrayView publicKey, QCryptographicHash::Algorithm digest,
		QByteArrayView algorithmID, QByteArrayView partyUInfo, QByteArrayView partyVInfo) const = 0;
	virtual std::vector<uint8_t> deriveHMACExtract(QByteArrayView publicKey, QByteArrayView salt, int keySize) const = 0;
	virtual bool isPSS() const = 0;
	virtual std::vector<uint8_t> sign(QCryptographicHash::Algorithm algo, Padding padding, QByteArrayView digest) const = 0;

	QSslCertificate cert() const;
	/**
	 * @brief Get a new Backend object and log in with the given token
	 *
	 * @param token the token to use
	 * @return the new backend object or an error code
	 */
	static std::expected<QCryptoBackend *,Status> getBackend(const TokenData &token, const QString &pin = {});

	/**
	 * @brief The status of the last operation
	 */
	mutable Status status = PinOK;

	static QString errorString(Status error, const TokenData &token = {});

protected:
	const TokenData &tokenData() const { return token; }
	virtual Status login(const TokenData &cert, const QString &pin) = 0;

private:
	TokenData token;
};

class QCryptoManager final : public QThread
{
	Q_OBJECT
public:
	explicit QCryptoManager();
	~QCryptoManager() final;

	QList<TokenData> cache() const;
	quint64 readCard(const TokenData &token, const QSmartCardData &previous);
	void refreshToken(const TokenData &token);

Q_SIGNALS:
	void cacheChanged(const QList<TokenData> &tokens);
	void tokenChanged(const TokenData &token);
	void cardDataReady(quint64 request, const QSmartCardData &data);
	void refreshed();

private:
	friend class QCryptoBackend;
	void refresh();
	void run() final;

	struct Private;
	Private *d;
};
