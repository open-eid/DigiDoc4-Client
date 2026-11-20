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

	std::vector<uint8_t> decrypt(QByteArrayView data, bool oaep) const final;
	std::vector<uint8_t> derive(QByteArrayView publicKey) const;
	std::vector<uint8_t> deriveConcatKDF(QByteArrayView publicKey, QCryptographicHash::Algorithm digest,
		QByteArrayView algorithmID, QByteArrayView partyUInfo, QByteArrayView partyVInfo) const final;
	std::vector<uint8_t> deriveHMACExtract(QByteArrayView publicKey, QByteArrayView salt, int keySize) const final;
	bool isPSS() const final;
	std::vector<uint8_t> sign(QCryptographicHash::Algorithm algo, Padding padding, QByteArrayView digest) const final;

	Status login(const TokenData &t, const QString &pin) final;

	static QList<TokenData> tokens();

private:
	struct Private;
	std::unique_ptr<Private> d;
};
