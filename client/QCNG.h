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

	Status login(const TokenData &token) final;

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

	struct Private;
	std::unique_ptr<Private> d;
};
