// SPDX-FileCopyrightText: Estonian Information System Authority
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "TokenData.h"

#include <QtCore/QCoreApplication>
#include <QtCore/QCryptographicHash>
#include <QtCore/QThread>

#include <expected>

class QSmartCard;
class QSslKey;

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
	static std::expected<QCryptoBackend *,Status> getBackend(const TokenData &token);

	/**
	 * @brief The status of the last operation
	 */
	mutable Status status = PinOK;

	static QString errorString(Status error);

protected:
	virtual Status login(const TokenData &cert) = 0;

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
	QSmartCard *smartcard() const;
	void selectCard(const TokenData &token);
	TokenData tokenauth() const;
	TokenData tokensign() const;

Q_SIGNALS:
	void cacheChanged();
	void authDataChanged(const TokenData &token);
	void signDataChanged(const TokenData &token);

private:
	friend class QCryptoBackend;
	void refresh();
	void run() final;

	struct Private;
	Private *d;
};
