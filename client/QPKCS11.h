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

#include <memory>

class QPKCS11 final: public QCryptoBackend
{
public:
	explicit QPKCS11();
	~QPKCS11() noexcept final;

	std::vector<uint8_t> decrypt(const QByteArray &data, bool oaep) const final;
	std::vector<uint8_t> derive(const QByteArray &publicKey) const;
	std::vector<uint8_t> deriveConcatKDF(const QByteArray &publicKey, QCryptographicHash::Algorithm digest,
		const QByteArray &algorithmID, const QByteArray &partyUInfo, const QByteArray &partyVInfo) const final;
	std::vector<uint8_t> deriveHMACExtract(const QByteArray &publicKey, const QByteArray &salt, int keySize) const final;
	QByteArray sign(QCryptographicHash::Algorithm type, const QByteArray &digest) const final;

	Status login(const TokenData &t) final;

	static QList<TokenData> tokens();

private:
	struct Private;
	std::unique_ptr<Private> d;
};
