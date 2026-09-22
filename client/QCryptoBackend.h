// SPDX-FileCopyrightText: Estonian Information System Authority
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "TokenData.h"

#include <QtCore/QCryptographicHash>

#include <expected>

class TokenData;
class QSslKey;

class QCryptoBackend
{
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

	virtual ~QCryptoBackend();

	virtual QByteArray decrypt(const QByteArray &data, bool oaep) const = 0;
	virtual QByteArray deriveConcatKDF(const QByteArray &publicKey, QCryptographicHash::Algorithm digest,
		const QByteArray &algorithmID, const QByteArray &partyUInfo, const QByteArray &partyVInfo) const = 0;
	virtual QByteArray deriveHMACExtract(const QByteArray &publicKey, const QByteArray &salt, int keySize) const = 0;
	virtual QByteArray sign(QCryptographicHash::Algorithm method, const QByteArray &digest) const = 0;

	/**
	 * @brief Get the SSL key for the certificate
	 * 
	 * @return the Qt SSL key
	 */
	QSslKey getKey() const;
	QSslCertificate cert() const;
	/**
	 * @brief Get a new Backend object and log in with the given token
	 * 
	 * @param token the token to use
	 * @return the new backend object or an error code 
	 */
	static std::expected<QCryptoBackend *,Status> getBackend(const TokenData& token);
	/**
	 * @brief Shut down all backends
	 * 
	 * This should be called when the application is about to exit. It releases all static data held by backend(s) (e.g. PKCS11 library)
	 */
	static void shutDown();

	/**
	 * @brief The status of the last operation
	 */
	mutable Status status = PinOK;

	/**
	 * @brief Get a list of all available tokens
	 * 
	 * @return list of all available tokens
	 */
	static QList<TokenData> getTokens();

	static QString errorString(Status error);
protected:
	virtual Status login(const TokenData &cert) = 0;

private:
	TokenData token;
};
