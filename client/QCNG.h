// SPDX-FileCopyrightText: Estonian Information System Authority
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "QCryptoBackend.h"
#include "TokenData.h"

#include <qt_windows.h>
#include <ncrypt.h>

class QCNG final: public QCryptoBackend
{
public:
	explicit QCNG() noexcept;
	~QCNG() noexcept final;

	Status login(const TokenData &token, const QString &pin) final;

	std::vector<uint8_t> decrypt(QByteArrayView data, bool oaep) const final;
	std::vector<uint8_t> deriveConcatKDF(QByteArrayView publicKey, QCryptographicHash::Algorithm digest,
		QByteArrayView algorithmID, QByteArrayView partyUInfo, QByteArrayView partyVInfo) const final;
	std::vector<uint8_t> deriveHMACExtract(QByteArrayView publicKey, QByteArrayView salt, int keySize) const final;
	bool isPSS() const final;
	std::vector<uint8_t> sign(QCryptographicHash::Algorithm algo, Padding padding, QByteArrayView digest) const final;

	static QList<TokenData> tokens();
private:
	template<typename F>
	std::vector<uint8_t> derive(QByteArrayView publicKey, F &&func) const;
	template<typename F>
	std::vector<uint8_t> exec(F &&func) const;

	struct Private;
	std::unique_ptr<Private> d;
};
