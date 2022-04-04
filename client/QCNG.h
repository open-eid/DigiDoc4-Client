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

#include "QCryptoBackend.h"

#include <functional>

#include <qt_windows.h>
#include <ncrypt.h>

class QCNG final: public QCryptoBackend
{
	Q_OBJECT
public:
	explicit QCNG(QObject *parent = nullptr);
	~QCNG() final;

	QList<TokenData> tokens() const final;
	QByteArray decrypt(const QByteArray &data, bool oaep) const final;
	QByteArray deriveConcatKDF(const QByteArray &publicKey, QCryptographicHash::Algorithm digest, int keySize,
		const QByteArray &algorithmID, const QByteArray &partyUInfo, const QByteArray &partyVInfo) const final;
	QByteArray deriveHMACExtract(const QByteArray &publicKey, const QByteArray &salt, int keySize) const final;
	PinStatus lastError() const final;
	PinStatus login(const TokenData &token) final;
	void logout() final {}
	QByteArray sign(QCryptographicHash::Algorithm type, const QByteArray &digest) const final;

private:
	QByteArray derive(const QByteArray &publicKey, std::function<long (NCRYPT_SECRET_HANDLE, QByteArray &)> &&func) const;
	QByteArray exec(std::function<long (NCRYPT_PROV_HANDLE, NCRYPT_KEY_HANDLE, QByteArray &)> &&func) const;

	class Private;
	Private *d;
};
