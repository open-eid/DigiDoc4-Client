/*
 * QDigiDoc4
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

#include "QCNG.h"

#include "SslCertificate.h"
#include "TokenData.h"

#include <common/QPCSC.h>

#include <QtCore/QDebug>

#include <openssl/obj_mac.h>

class QCNG::Private
{
public:
	TokenData token;
	QCNG::PinStatus err = QCNG::PinOK;
};

QCNG::QCNG( QObject *parent )
	: QCryptoBackend(parent)
	, d(new Private)
{}

QCNG::~QCNG()
{
	delete d;
}

QByteArray QCNG::decrypt(const QByteArray &data) const
{
	return exec([&](NCRYPT_PROV_HANDLE prov, NCRYPT_KEY_HANDLE key, QByteArray &result) {
		DWORD size = DWORD(data.size());
		result.resize(int(size));
		SECURITY_STATUS err = NCryptDecrypt(key, PBYTE(data.constData()), DWORD(data.size()), nullptr,
			PBYTE(result.data()), DWORD(result.size()), &size, NCRYPT_PAD_PKCS1_FLAG);
		if(SUCCEEDED(err))
			result.resize(int(size));
		return err;
	});
}

QByteArray QCNG::derive(const QByteArray &publicKey, std::function<long (NCRYPT_SECRET_HANDLE, QByteArray &)> &&func) const
{
	return exec([&](NCRYPT_PROV_HANDLE prov, NCRYPT_KEY_HANDLE key, QByteArray &derived) {
		BCRYPT_ECCKEY_BLOB oh { BCRYPT_ECDH_PUBLIC_P384_MAGIC, ULONG((publicKey.size() - 1) / 2) };
		switch((publicKey.size() - 1) * 4)
		{
		case 256: oh.dwMagic = BCRYPT_ECDH_PUBLIC_P256_MAGIC; break;
		case 384: oh.dwMagic = BCRYPT_ECDH_PUBLIC_P384_MAGIC; break;
		default: oh.dwMagic = BCRYPT_ECDH_PUBLIC_P521_MAGIC; break;
		}
		QByteArray blob((char*)&oh, sizeof(BCRYPT_ECCKEY_BLOB));
		blob += publicKey.mid(1);
		NCRYPT_KEY_HANDLE publicKeyHandle = 0;
		NCRYPT_SECRET_HANDLE sharedSecret = 0;
		SECURITY_STATUS err = 0;
		if(SUCCEEDED(err = NCryptImportKey(prov, NULL, BCRYPT_ECCPUBLIC_BLOB, nullptr, &publicKeyHandle, PBYTE(blob.data()), DWORD(blob.size()), 0)))
			err = NCryptSecretAgreement(key, publicKeyHandle, &sharedSecret, 0);
		if(publicKeyHandle)
			NCryptFreeObject(publicKeyHandle);
		if(FAILED(err))
			return err;
		err = func(sharedSecret, derived);
		NCryptFreeObject(sharedSecret);
		return err;
	});
}

QByteArray QCNG::deriveConcatKDF(const QByteArray &publicKey, QCryptographicHash::Algorithm digest, int keySize,
	const QByteArray &algorithmID, const QByteArray &partyUInfo, const QByteArray &partyVInfo) const
{
	return derive(publicKey, [&](NCRYPT_SECRET_HANDLE sharedSecret, QByteArray &derived) {
		QVector<BCryptBuffer> paramValues{
			{ULONG(algorithmID.size()), KDF_ALGORITHMID, PBYTE(algorithmID.data())},
			{ULONG(partyUInfo.size()), KDF_PARTYUINFO, PBYTE(partyUInfo.data())},
			{ULONG(partyVInfo.size()), KDF_PARTYVINFO, PBYTE(partyVInfo.data())},
		};
		switch(digest)
		{
		case QCryptographicHash::Sha256:
			paramValues.push_back({ULONG(sizeof(BCRYPT_SHA256_ALGORITHM)), KDF_HASH_ALGORITHM, PBYTE(BCRYPT_SHA256_ALGORITHM)}); break;
		case QCryptographicHash::Sha384:
			paramValues.push_back({ULONG(sizeof(BCRYPT_SHA384_ALGORITHM)), KDF_HASH_ALGORITHM, PBYTE(BCRYPT_SHA384_ALGORITHM)}); break;
		case QCryptographicHash::Sha512:
			paramValues.push_back({ULONG(sizeof(BCRYPT_SHA512_ALGORITHM)), KDF_HASH_ALGORITHM, PBYTE(BCRYPT_SHA512_ALGORITHM)}); break;
		default: return NTE_INVALID_PARAMETER;
		}
		BCryptBufferDesc params{ BCRYPTBUFFER_VERSION };
		params.cBuffers = ULONG(paramValues.size());
		params.pBuffers = paramValues.data();
		DWORD size = 0;
		SECURITY_STATUS err = 0;
		if(FAILED(err = NCryptDeriveKey(sharedSecret, BCRYPT_KDF_SP80056A_CONCAT, &params, nullptr, 0, &size, 0)))
			return err;
		derived.resize(int(size));
		if(SUCCEEDED(err = NCryptDeriveKey(sharedSecret, BCRYPT_KDF_SP80056A_CONCAT, &params, PBYTE(derived.data()), size, &size, 0)))
			derived.resize(keySize);
		return err;
	});
}

QByteArray QCNG::exec(std::function<long (NCRYPT_PROV_HANDLE, NCRYPT_KEY_HANDLE, QByteArray &)> &&func) const
{
	d->err = UnknownError;
	NCRYPT_PROV_HANDLE prov = 0;
	if(FAILED(NCryptOpenStorageProvider(&prov, LPCWSTR(d->token.data(QStringLiteral("provider")).toString().utf16()), 0)))
		return {};
	NCRYPT_KEY_HANDLE key = 0;
	if(FAILED(NCryptOpenKey(prov, &key, LPWSTR(d->token.data(QStringLiteral("key")).toString().utf16()),
		d->token.data(QStringLiteral("spec")).value<DWORD>(), 0)))
	{
		NCryptFreeObject(prov);
		return {};
	}
	// https://docs.microsoft.com/en-us/archive/blogs/alejacma/smart-cards-pin-gets-cached
	NCryptSetProperty(key, NCRYPT_PIN_PROPERTY, nullptr, 0, 0);
	QByteArray result;
	long err = func(prov, key, result);
	NCryptFreeObject(key);
	NCryptFreeObject(prov);
	switch(err)
	{
	case ERROR_SUCCESS:
		d->err = PinOK;
		return result;
	case SCARD_W_CANCELLED_BY_USER:
	case ERROR_CANCELLED:
		d->err = PinCanceled;
	default:
		return {};
	}
}

QCNG::PinStatus QCNG::lastError() const { return d->err; }

QList<TokenData> QCNG::tokens() const
{
	QList<TokenData> result;
	auto prop = [](NCRYPT_HANDLE handle, LPCWSTR param) -> QByteArray {
		if(!handle)
			return {};
		DWORD size = 0;
		if(NCryptGetProperty(handle, param, nullptr, 0, &size, 0))
			return {};
		QByteArray data(int(size), '\0');
		if(NCryptGetProperty(handle, param, PBYTE(data.data()), size, &size, 0))
			data.clear();
		return data;
	};
	auto enumKeys = [&result, &prop](const QString &provider, const QString &reader = {}) {
		QString scope = QStringLiteral(R"(\\.\%1\)").arg(reader);
		NCRYPT_PROV_HANDLE h = 0;
		SECURITY_STATUS err = NCryptOpenStorageProvider(&h, LPCWSTR(provider.utf16()), 0);
		NCryptKeyName *keyname = nullptr;
		PVOID pos = nullptr;
		while(NCryptEnumKeys(h, reader.isEmpty() ? nullptr : LPCWSTR(scope.utf16()), &keyname, &pos, NCRYPT_SILENT_FLAG) == ERROR_SUCCESS)
		{
			NCRYPT_KEY_HANDLE key = 0;
			err = NCryptOpenKey(h, &key, keyname->pszName, keyname->dwLegacyKeySpec, NCRYPT_SILENT_FLAG);
			SslCertificate cert(prop(key, NCRYPT_CERTIFICATE_PROPERTY), QSsl::Der);
			if(!cert.isNull())
			{
				QString guid = prop(h, NCRYPT_SMARTCARD_GUID_PROPERTY).trimmed();
				TokenData t;
				t.setReader(reader);
				t.setCard(cert.type() & SslCertificate::EstEidType || cert.type() & SslCertificate::DigiIDType ?
					guid : cert.subjectInfo(QSslCertificate::CommonName) + "-" + cert.serialNumber());
				t.setCert(cert);
				t.setData(QStringLiteral("provider"), provider);
				t.setData(QStringLiteral("key"), QString::fromWCharArray(keyname->pszName));
				t.setData(QStringLiteral("spec"), QVariant::fromValue(keyname->dwLegacyKeySpec));
				qWarning() << "key" << t.data(QStringLiteral("provider"))
					<< "spec" << t.data(QStringLiteral("spec"))
					<< "alg" << QString::fromWCharArray(keyname->pszAlgid)
					<< "flags" << keyname->dwFlags;
				result << t;
			}
			NCryptFreeObject(key);
			NCryptFreeBuffer(keyname);
			keyname = nullptr;
		}
		NCryptFreeObject(h);
	};

	qWarning() << "Start enumerationg providers";
	DWORD count = 0;
	NCryptProviderName *providers = nullptr;
	NCryptEnumStorageProviders(&count, &providers, NCRYPT_SILENT_FLAG);
	for( DWORD i = 0; i < count; ++i )
	{
		QString provider = QString::fromWCharArray(providers[i].pszName);
		qWarning() << "Found provider" << provider;
		if(provider == QString::fromWCharArray(MS_SMART_CARD_KEY_STORAGE_PROVIDER))
		{
			for( const QString &reader: QPCSC::instance().readers() )
			{
				qWarning() << reader;
				enumKeys(provider, reader);
			}
		}
		else
			enumKeys(provider);
	}
	NCryptFreeBuffer(providers);
	qWarning() << "End enumerationg providers";

	return result;
}

QCNG::PinStatus QCNG::login(const TokenData &token)
{
	d->token = token;
	return d->err = QCNG::PinOK;
}

QByteArray QCNG::sign( int method, const QByteArray &digest ) const
{
	return exec([&](NCRYPT_PROV_HANDLE prov, NCRYPT_KEY_HANDLE key, QByteArray &result) {
		BCRYPT_PKCS1_PADDING_INFO padInfo = { NCRYPT_SHA256_ALGORITHM };
		switch( method )
		{
		case NID_sha224: padInfo.pszAlgId = L"SHA224"; break;
		case NID_sha256: padInfo.pszAlgId = NCRYPT_SHA256_ALGORITHM; break;
		case NID_sha384: padInfo.pszAlgId = NCRYPT_SHA384_ALGORITHM; break;
		case NID_sha512: padInfo.pszAlgId = NCRYPT_SHA512_ALGORITHM; break;
		default: return NTE_INVALID_PARAMETER;
		}
		DWORD size = 0;
		QString algo(5, '\0');
		NCryptGetProperty(key, NCRYPT_ALGORITHM_GROUP_PROPERTY, PBYTE(algo.data()), DWORD((algo.size() + 1) * 2), &size, 0);
		algo.resize(size/2 - 1);
		bool isRSA = algo == QLatin1String("RSA");
		SECURITY_STATUS err = NCryptSignHash(key, isRSA ? &padInfo : nullptr, PBYTE(digest.constData()), DWORD(digest.size()),
			nullptr, 0, &size, isRSA ? BCRYPT_PAD_PKCS1 : 0);
		if(FAILED(err))
			return err;
		result.resize(int(size));
		return NCryptSignHash(key, isRSA ? &padInfo : nullptr, PBYTE(digest.constData()), DWORD(digest.size()),
			PBYTE(result.data()), DWORD(result.size()), &size, isRSA ? BCRYPT_PAD_PKCS1 : 0);
	});
}
