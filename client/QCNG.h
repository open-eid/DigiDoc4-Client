// SPDX-FileCopyrightText: Estonian Information System Authority
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "QCryptoBackend.h"

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
	QByteArray deriveConcatKDF(const QByteArray &publicKey, QCryptographicHash::Algorithm digest,
		const QByteArray &algorithmID, const QByteArray &partyUInfo, const QByteArray &partyVInfo) const final;
	QByteArray deriveHMACExtract(const QByteArray &publicKey, const QByteArray &salt, int keySize) const final;
	PinStatus lastError() const final;
	PinStatus login(const TokenData &token) final;
	void logout() final {}
	QByteArray sign(QCryptographicHash::Algorithm type, const QByteArray &digest) const final;

private:
	template<typename F>
	QByteArray derive(const QByteArray &publicKey, F &&func) const;
	template<typename F>
	QByteArray exec(F &&func) const;

	class Private;
	Private *d;
};
