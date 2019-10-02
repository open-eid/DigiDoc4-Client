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

#include <QObject>

#include <qt_windows.h>
#include <ncrypt.h>

class TokenData;
class SslCertificate;

class QWin: public QObject
{
	Q_OBJECT
public:
	enum PinStatus
	{
		PinOK,
		PinCanceled,
		PinUnknown,
	};

	explicit QWin(QObject *parent = nullptr): QObject(parent) {}
	~QWin() override = default;

	virtual QList<TokenData> tokens() const = 0;
	virtual QByteArray decrypt( const QByteArray &data ) = 0;
	virtual QByteArray deriveConcatKDF(const QByteArray &publicKey, const QString &digest, int keySize,
		const QByteArray &algorithmID, const QByteArray &partyUInfo, const QByteArray &partyVInfo) const = 0;
	virtual PinStatus lastError() const = 0;
	virtual TokenData selectCert( const SslCertificate &cert ) = 0;
	virtual QByteArray sign( int method, const QByteArray &digest ) const = 0;

protected:
	int derive(NCRYPT_PROV_HANDLE prov, NCRYPT_KEY_HANDLE key, const QByteArray &publicKey, const QString &digest, int keySize,
		const QByteArray &algorithmID, const QByteArray &partyUInfo, const QByteArray &partyVInfo, QByteArray &derived) const;

	NCRYPT_HANDLE keyProvider(NCRYPT_HANDLE key) const;
	static QByteArray prop(NCRYPT_HANDLE handle, LPCWSTR param, DWORD flags = 0);
};
