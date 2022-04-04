/*
 * QDigiDocClient
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
