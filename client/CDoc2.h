// SPDX-FileCopyrightText: Estonian Information System Authority
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "CryptoDoc.h"

#include <QtCore/QFile>

class CDoc2 final: public CDoc, private QFile {
public:
	explicit CDoc2() = default;
	explicit CDoc2(const QString &path);

	CKey canDecrypt(const QSslCertificate &cert) const final;
	bool decryptPayload(const QByteArray &fmk) final;
	QByteArray deriveFMK(const QByteArray &priv, const CKey &key);
	bool isSupported();
	bool save(const QString &path) final;
	QByteArray transportKey(const CKey &key) final;
	int version() final;

private:
	QByteArray header_data, headerHMAC;
	qint64 noncePos = -1;

	static const QByteArray LABEL, CEK, HMAC, KEK, KEKPREMASTER, PAYLOAD, SALT;
	static constexpr int KEY_LEN = 32, NONCE_LEN = 12;
};
