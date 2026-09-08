// SPDX-FileCopyrightText: Estonian Information System Authority
// SPDX-License-Identifier: LGPL-2.1-or-later

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

	Status login(const TokenData &t, const QString &pin) final;

	static QList<TokenData> tokens();

private:
	struct Private;
	std::unique_ptr<Private> d;
};
