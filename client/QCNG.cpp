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
#include <QtNetwork/QSslKey>

template<typename T, typename D = decltype(NCryptFreeObject)>
struct SCOPE
{
	T d {};
	~SCOPE() { if(d) D(d); }
	inline operator T() const { return d; }
	inline T* operator&() { return &d; }
};

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
		auto size = DWORD(data.size());
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
		SCOPE<NCRYPT_KEY_HANDLE> publicKeyHandle;
		SCOPE<NCRYPT_SECRET_HANDLE> sharedSecret;
		SECURITY_STATUS err {};
		if(SUCCEEDED(err = NCryptImportKey(prov, NULL, BCRYPT_ECCPUBLIC_BLOB, nullptr, &publicKeyHandle, PBYTE(blob.data()), DWORD(blob.size()), 0)))
			err = NCryptSecretAgreement(key, publicKeyHandle, &sharedSecret, 0);
		if(FAILED(err))
			return err;
		return func(sharedSecret, derived);
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
		BCryptBufferDesc params{ BCRYPTBUFFER_VERSION,  ULONG(paramValues.size()), paramValues.data() };
		DWORD size {};
		SECURITY_STATUS err {};
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
	SCOPE<NCRYPT_PROV_HANDLE> prov;
	if(FAILED(NCryptOpenStorageProvider(&prov, LPCWSTR(d->token.data(QStringLiteral("provider")).toString().utf16()), 0)))
		return {};
	SCOPE<NCRYPT_KEY_HANDLE> key;
	if(FAILED(NCryptOpenKey(prov, &key, LPWSTR(d->token.data(QStringLiteral("key")).toString().utf16()),
		d->token.data(QStringLiteral("spec")).value<DWORD>(), 0)))
		return {};
	// https://docs.microsoft.com/en-us/archive/blogs/alejacma/smart-cards-pin-gets-cached
	NCryptSetProperty(key, NCRYPT_PIN_PROPERTY, nullptr, 0, 0);
	QByteArray result;
	switch(func(prov, key, result))
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
		DWORD size {};
		if(NCryptGetProperty(handle, param, nullptr, 0, &size, 0))
			return {};
		QByteArray data(int(size), '\0');
		if(NCryptGetProperty(handle, param, PBYTE(data.data()), size, &size, 0))
			return {};
		return data;
	};
	auto enumKeys = [&result, &prop](const QString &provider, QString reader = {}) {
		QString scope = QStringLiteral(R"(\\.\%1\)").arg(reader);
		SCOPE<NCRYPT_PROV_HANDLE> h;
		SECURITY_STATUS err = NCryptOpenStorageProvider(&h, LPCWSTR(provider.utf16()), 0);
		NCryptKeyName *keyname {};
		PVOID pos {};
		BCRYPT_PSS_PADDING_INFO rsaPSS { NCRYPT_SHA256_ALGORITHM, 32 };
		DWORD size {};
		QByteArray digest = QCryptographicHash::hash({0}, QCryptographicHash::Sha256);
		while(SUCCEEDED(NCryptEnumKeys(h, reader.isEmpty() ? nullptr : LPCWSTR(scope.utf16()), &keyname, &pos, NCRYPT_SILENT_FLAG)))
		{
			SCOPE<NCryptKeyName*,decltype(NCryptFreeBuffer)> keyname_scope{keyname};
			SCOPE<NCRYPT_KEY_HANDLE> key;
			err = NCryptOpenKey(h, &key, keyname->pszName, keyname->dwLegacyKeySpec, NCRYPT_SILENT_FLAG);
			SslCertificate cert(prop(key, NCRYPT_CERTIFICATE_PROPERTY), QSsl::Der);
			if(cert.isNull())
				continue;

			if(reader.isEmpty())
				reader = QString::fromUtf16((const char16_t*)prop(key, NCRYPT_READER_PROPERTY).data());
			QString guid = prop(h, NCRYPT_SMARTCARD_GUID_PROPERTY).trimmed();
			TokenData &t = result.emplaceBack();
			t.setReader(reader);
			t.setCard(cert.type() & SslCertificate::EstEidType || cert.type() & SslCertificate::DigiIDType ?
				guid : cert.subjectInfo(QSslCertificate::CommonName) + "-" + cert.serialNumber());
			t.setCert(cert);
			t.setData(QStringLiteral("provider"), provider);
			t.setData(QStringLiteral("key"), QString::fromWCharArray(keyname->pszName));
			t.setData(QStringLiteral("spec"), QVariant::fromValue(keyname->dwLegacyKeySpec));
			qWarning() << "key" << t.data(QStringLiteral("provider"))
				<< "spec" << t.data(QStringLiteral("spec"))
				<< "alg" << QStringView(keyname->pszAlgid)
				<< "flags" << keyname->dwFlags;
			if(cert.publicKey().algorithm() != QSsl::Rsa || reader.isEmpty())
				continue;

			static const QSet<QByteArray> usePSS {
				{"3BFF9600008131804380318065B0850300EF120FFE82900066"}, // eToken 5110 CC (830)
				{"3BFF9600008131FE4380318065B0855956FB120FFE82900000"}, // eToken 5110 CC (940)
				{"3BD518008131FE7D8073C82110F4"}, // SafeNet 5110 FIPS
				{"3BFF9600008131FE4380318065B0846566FB12017882900085"}, // SafeNet 5110+ FIPS
			};
			t.setData(QStringLiteral("PSS"), usePSS.contains(QPCSCReader(reader, &QPCSC::instance()).atr()));
		}
	};

	qWarning() << "Start enumerationg providers";
	DWORD count {};
	NCryptProviderName *providers {};
	NCryptEnumStorageProviders(&count, &providers, NCRYPT_SILENT_FLAG);
	for(DWORD i {}; i < count; ++i)
	{
		QString provider = QString::fromWCharArray(providers[i].pszName);
		qWarning() << "Found provider" << provider;
		if(provider == QStringView(MS_SMART_CARD_KEY_STORAGE_PROVIDER))
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

QByteArray QCNG::sign(QCryptographicHash::Algorithm type, const QByteArray &digest) const
{
	return exec([&](NCRYPT_PROV_HANDLE prov, NCRYPT_KEY_HANDLE key, QByteArray &result) {
		BCRYPT_PSS_PADDING_INFO rsaPSS { NCRYPT_SHA256_ALGORITHM, 32 };
		switch(type)
		{
		case QCryptographicHash::Sha224: rsaPSS = { L"SHA224", 24 }; break;
		case QCryptographicHash::Sha256: rsaPSS = { NCRYPT_SHA256_ALGORITHM, 32 }; break;
		case QCryptographicHash::Sha384: rsaPSS = { NCRYPT_SHA384_ALGORITHM, 48 }; break;
		case QCryptographicHash::Sha512: rsaPSS = { NCRYPT_SHA256_ALGORITHM, 64 }; break;
		default: return NTE_INVALID_PARAMETER;
		}
		BCRYPT_PKCS1_PADDING_INFO rsaPKCS1 { rsaPSS.pszAlgId };
		DWORD size {};
		QString algo(5, '\0');
		NCryptGetProperty(key, NCRYPT_ALGORITHM_GROUP_PROPERTY, PBYTE(algo.data()), DWORD((algo.size() + 1) * 2), &size, 0);
		algo.resize(size/2 - 1);
		bool isRSA = algo == QLatin1String("RSA");
		DWORD padding {};
		PVOID paddingInfo {};
		if(isRSA && d->token.data(QStringLiteral("PSS")).toBool())
		{
			padding = BCRYPT_PAD_PSS;
			paddingInfo = &rsaPSS;
		}
		else if(isRSA)
		{
			padding = BCRYPT_PAD_PKCS1;
			paddingInfo = &rsaPKCS1;
		}
		SECURITY_STATUS err = NCryptSignHash(key, paddingInfo, PBYTE(digest.constData()), DWORD(digest.size()),
			nullptr, 0, &size, padding);
		if(FAILED(err))
			return err;
		result.resize(int(size));
		return NCryptSignHash(key, paddingInfo, PBYTE(digest.constData()), DWORD(digest.size()),
			PBYTE(result.data()), DWORD(result.size()), &size, padding);
	});
}
