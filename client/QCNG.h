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
#include "TokenData.h"

#include <qt_windows.h>
#include <ncrypt.h>

class QCNG final: public QCryptoBackend
{
public:
	explicit QCNG();
	~QCNG() final;

	Status login(const TokenData &token) final;
	void logout() final {}

	QByteArray decrypt(const QByteArray &data, bool oaep) const final;
	QByteArray deriveConcatKDF(const QByteArray &publicKey, QCryptographicHash::Algorithm digest,
		const QByteArray &algorithmID, const QByteArray &partyUInfo, const QByteArray &partyVInfo) const final;
	QByteArray deriveHMACExtract(const QByteArray &publicKey, const QByteArray &salt, int keySize) const final;
	QByteArray sign(QCryptographicHash::Algorithm type, const QByteArray &digest) const final;

	static QList<TokenData> tokens();
private:
	template<typename F>
	QByteArray derive(const QByteArray &publicKey, F &&func) const;
	template<typename F>
	QByteArray exec(F &&func) const;

	TokenData token;
};
