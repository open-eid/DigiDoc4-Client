// SPDX-FileCopyrightText: Estonian Information System Authority
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include <QObject>
#include <QCryptographicHash>

class TokenData;

class QCryptoBackend: public QObject
{
	Q_OBJECT
public:
	enum PinStatus : quint8
	{
		PinOK,
		PinCanceled,
		PinIncorrect,
		PinLocked,
		DeviceError,
		GeneralError,
		UnknownError
	};

	using QObject::QObject;

	virtual QList<TokenData> tokens() const = 0;
	virtual QByteArray decrypt(const QByteArray &data, bool oaep) const = 0;
	virtual QByteArray deriveConcatKDF(const QByteArray &publicKey, QCryptographicHash::Algorithm digest,
		const QByteArray &algorithmID, const QByteArray &partyUInfo, const QByteArray &partyVInfo) const = 0;
	virtual QByteArray deriveHMACExtract(const QByteArray &publicKey, const QByteArray &salt, int keySize) const = 0;
	virtual PinStatus lastError() const { return PinOK; }
	virtual PinStatus login(const TokenData &cert) = 0;
	virtual void logout() = 0;
	virtual QByteArray sign(QCryptographicHash::Algorithm method, const QByteArray &digest) const = 0;

	static QString errorString( PinStatus error );
};
