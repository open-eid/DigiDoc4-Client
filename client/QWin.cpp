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

#include "QWin.h"

#include <common/QPCSC.h>

#include <QtCore/QEventLoop>
#include <QtCore/QVector>

#include <wincrypt.h>

#include <thread>

int QWin::derive(NCRYPT_PROV_HANDLE prov, NCRYPT_KEY_HANDLE key, const QByteArray &publicKey, const QString &digest, int keySize,
	const QByteArray &algorithmID, const QByteArray &partyUInfo, const QByteArray &partyVInfo, QByteArray &derived) const
{
	if(!prov)
		return NTE_INVALID_HANDLE;
	BCRYPT_ECCKEY_BLOB oh = { BCRYPT_ECDH_PUBLIC_P384_MAGIC, ULONG((publicKey.size() - 1) / 2) };
	switch ((publicKey.size() - 1) * 4)
	{
	case 256: oh.dwMagic = BCRYPT_ECDH_PUBLIC_P256_MAGIC; break;
	case 384: oh.dwMagic = BCRYPT_ECDH_PUBLIC_P384_MAGIC; break;
	case 521: oh.dwMagic = BCRYPT_ECDH_PUBLIC_P521_MAGIC; break;
	default:break;
	}
	QByteArray blob((char*)&oh, sizeof(BCRYPT_ECCKEY_BLOB));
	blob += publicKey.mid(1);

	NCRYPT_KEY_HANDLE publicKeyHandle = 0;
	NCRYPT_SECRET_HANDLE sharedSecret = 0;
	SECURITY_STATUS err = 0;
	QEventLoop e;
	std::thread([&]{
		if((err = NCryptImportKey(prov, NULL, BCRYPT_ECCPUBLIC_BLOB, nullptr, &publicKeyHandle, PBYTE(blob.data()), DWORD(blob.size()), 0)))
			return e.exit();
		err = NCryptSecretAgreement(key, publicKeyHandle, &sharedSecret, 0);
		e.exit();
	}).detach();
	e.exec();
	if(err)
	{
		if(publicKeyHandle)
			NCryptFreeObject(publicKeyHandle);
		NCryptFreeObject(prov);
		NCryptFreeObject(key);
		return err;
	}

	QVector<BCryptBuffer> paramValues{
		{ULONG(algorithmID.size()), KDF_ALGORITHMID, PBYTE(algorithmID.data())},
		{ULONG(partyUInfo.size()), KDF_PARTYUINFO, PBYTE(partyUInfo.data())},
		{ULONG(partyVInfo.size()), KDF_PARTYVINFO, PBYTE(partyVInfo.data())},
	};
	if(digest == QStringLiteral("http://www.w3.org/2001/04/xmlenc#sha256"))
		paramValues.push_back({ULONG(sizeof(BCRYPT_SHA256_ALGORITHM)), KDF_HASH_ALGORITHM, PBYTE(BCRYPT_SHA256_ALGORITHM)});
	if(digest == QStringLiteral("http://www.w3.org/2001/04/xmlenc#sha384"))
		paramValues.push_back({ULONG(sizeof(BCRYPT_SHA384_ALGORITHM)), KDF_HASH_ALGORITHM, PBYTE(BCRYPT_SHA384_ALGORITHM)});
	if(digest == QStringLiteral("http://www.w3.org/2001/04/xmlenc#sha512"))
		paramValues.push_back({ULONG(sizeof(BCRYPT_SHA512_ALGORITHM)), KDF_HASH_ALGORITHM, PBYTE(BCRYPT_SHA512_ALGORITHM)});
	BCryptBufferDesc params;
	params.ulVersion = BCRYPTBUFFER_VERSION;
	params.cBuffers = ULONG(paramValues.size());
	params.pBuffers = paramValues.data();

	DWORD size = 0;
	if((err = NCryptDeriveKey(sharedSecret, BCRYPT_KDF_SP80056A_CONCAT, &params, nullptr, 0, &size, 0)) == 0)
	{
		derived.resize(int(size));
		if((err = NCryptDeriveKey(sharedSecret, BCRYPT_KDF_SP80056A_CONCAT, &params, PBYTE(derived.data()), size, &size, 0)) == 0)
			derived.resize(keySize);
		else
			derived.clear();
	}

	NCryptFreeObject(publicKeyHandle);
	NCryptFreeObject(sharedSecret);
	NCryptFreeObject(prov);
	return err;
}

NCRYPT_HANDLE QWin::keyProvider(NCRYPT_HANDLE key) const
{
	NCRYPT_PROV_HANDLE provider = 0;
	DWORD size = 0;
	NCryptGetProperty(key, NCRYPT_PROVIDER_HANDLE_PROPERTY, PBYTE(&provider), sizeof(provider), &size, 0);
	return provider;
}

QByteArray QWin::prop(NCRYPT_HANDLE handle, LPCWSTR param, DWORD flags)
{
	QByteArray data;
	if(!handle)
		return data;
	DWORD size = 0;
	if(NCryptGetProperty(handle, param, nullptr, 0, &size, flags))
		return data;
	data.resize(int(size));
	if(NCryptGetProperty(handle, param, PBYTE(data.data()), size, &size, flags))
		data.clear();
	return data;
}
