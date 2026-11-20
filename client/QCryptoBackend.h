// SPDX-FileCopyrightText: Estonian Information System Authority
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "TokenData.h"
#include "QSmartCard.h"

#include <QtCore/QCoreApplication>
#include <QtCore/QCryptographicHash>
#include <QtCore/QThread>

#include <cstdint>
#include <expected>
#include <vector>

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

	static QString errorString(Status error);

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
