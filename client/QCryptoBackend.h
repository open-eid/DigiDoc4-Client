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

#include <QObject>
#include <QCryptographicHash>
#include <QtNetwork/QSslKey>
#include <QtNetwork/QSslCertificate>

#include <expected>

class TokenData;

class QCryptoBackend
{
public:
	enum Status : quint8
	{
		PinOK,
		PinCanceled,
		PinIncorrect,
		PinLocked,
		DeviceError,
		GeneralError,
		UnknownError
	};

	virtual ~QCryptoBackend() {};

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
	QSslKey getKey();
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
	 * @brief Get a list of all available tokens
	 * 
	 * @return list of all available tokens
	 */
	static QList<TokenData> getTokens();

	static QString errorString(Status error);
protected:
	virtual Status login(const TokenData &cert) = 0;
	virtual void logout() = 0;

	QSslCertificate cert;
};
