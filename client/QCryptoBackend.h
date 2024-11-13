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
