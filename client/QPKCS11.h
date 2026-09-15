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

	QByteArray decrypt(const QByteArray &data, bool oaep) const final;
	QByteArray derive(const QByteArray &publicKey) const;
	QByteArray deriveConcatKDF(const QByteArray &publicKey, QCryptographicHash::Algorithm digest,
		const QByteArray &algorithmID, const QByteArray &partyUInfo, const QByteArray &partyVInfo) const final;
	QByteArray deriveHMACExtract(const QByteArray &publicKey, const QByteArray &salt, int keySize) const final;
	QByteArray sign(QCryptographicHash::Algorithm type, const QByteArray &digest) const final;

	Status login(const TokenData &t) final;

	static QList<TokenData> tokens();

private:
	struct Private;
	std::unique_ptr<Private> d;
};
