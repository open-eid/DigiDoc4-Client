// SPDX-FileCopyrightText: Estonian Information System Authority
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "QCryptoBackend.h"

class QPKCS11 final: public QCryptoBackend
{
	Q_OBJECT
public:
	explicit QPKCS11(QObject *parent = nullptr);
	~QPKCS11() final;

	QByteArray decrypt(const QByteArray &data, bool oaep) const final;
	QByteArray derive(const QByteArray &publicKey) const;
	QByteArray deriveConcatKDF(const QByteArray &publicKey, QCryptographicHash::Algorithm digest,
		const QByteArray &algorithmID, const QByteArray &partyUInfo, const QByteArray &partyVInfo) const final;
	QByteArray deriveHMACExtract(const QByteArray &publicKey, const QByteArray &salt, int keySize) const final;
	bool isLoaded() const;
	bool load( const QString &driver );
	void unload();
	PinStatus login(const TokenData &t) final;
	void logout() final;
	bool reload();
	QByteArray sign(QCryptographicHash::Algorithm type, const QByteArray &digest) const final;
	QList<TokenData> tokens() const final;
private:
	class Private;
	Private *d;
};
