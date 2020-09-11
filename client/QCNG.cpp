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

#include <wincrypt.h>

#include <openssl/obj_mac.h>

#include <thread>

class QCNG::Private
{
public:
	NCRYPT_KEY_HANDLE key() const
	{
		NCRYPT_PROV_HANDLE prov = 0;
		NCryptOpenStorageProvider(&prov, LPCWSTR(token.data(QStringLiteral("provider")).toString().utf16()), 0);
		NCRYPT_KEY_HANDLE key = 0;
		NCryptOpenKey(prov, &key, LPWSTR(token.data(QStringLiteral("key")).toString().utf16()),
			token.data(QStringLiteral("spec")).value<DWORD>(), 0);
		NCryptFreeObject( prov );
		return key;
	}

	TokenData token;
	QCNG::PinStatus err;
};



QCNG::QCNG( QObject *parent )
:	QWin( parent )
,	d( new Private )
{
	d->err = QCNG::PinOK;
}

QCNG::~QCNG()
{
	delete d;
}

QByteArray QCNG::decrypt(const QByteArray &data) const
{
	d->err = PinOK;
	DWORD size = 256;
	QByteArray res(int(size), 0);
	NCRYPT_KEY_HANDLE k = d->key();
	SECURITY_STATUS err = 0;
	QEventLoop e;
	std::thread([&]{
		err = NCryptDecrypt(k, PBYTE(data.constData()), DWORD(data.size()), nullptr,
			PBYTE(res.data()), DWORD(res.size()), &size, NCRYPT_PAD_PKCS1_FLAG);
		e.exit();
	}).detach();
	e.exec();
	NCryptFreeObject( k );
	switch( err )
	{
	case ERROR_SUCCESS:
		d->err = PinOK;
		res.resize(int(size));
		return res;
	case SCARD_W_CANCELLED_BY_USER:
		d->err = PinCanceled; break;
	default: break;
	}
	return QByteArray();
}

QByteArray QCNG::deriveConcatKDF(const QByteArray &publicKey, const QString &digest, int keySize,
	const QByteArray &algorithmID, const QByteArray &partyUInfo, const QByteArray &partyVInfo) const
{
	d->err = PinOK;
	QByteArray derived;
	NCRYPT_PROV_HANDLE prov = 0;
	if(NCryptOpenStorageProvider(&prov, LPCWSTR(d->token.data(QStringLiteral("provider")).toString().utf16()), 0))
		return derived;
	NCRYPT_KEY_HANDLE key = 0;
	if(NCryptOpenKey(prov, &key, LPWSTR(d->token.data(QStringLiteral("key")).toString().utf16()),
			d->token.data(QStringLiteral("spec")).value<DWORD>(), 0))
	{
		NCryptFreeObject(prov);
		return derived;
	}
	int err = derive(prov, key, publicKey, digest, keySize, algorithmID, partyUInfo, partyVInfo, derived);
	NCryptFreeObject(key);
	switch(err)
	{
	case ERROR_SUCCESS:
		d->err = PinOK;
		return derived;
	case SCARD_W_CANCELLED_BY_USER:
		d->err = PinCanceled;
	default:
		return derived;
	}
}

QCNG::PinStatus QCNG::lastError() const { return d->err; }

QList<TokenData> QCNG::tokens() const
{
	QList<TokenData> result;
	auto enumKeys = [&result](const QString &provider, const QString &reader = {}) {
		QString scope = QStringLiteral(R"(\\.\%1\)").arg(reader);
		NCRYPT_PROV_HANDLE h = 0;
		SECURITY_STATUS err = NCryptOpenStorageProvider(&h, LPCWSTR(provider.utf16()), 0);
		NCryptKeyName *keyname = nullptr;
		PVOID pos = nullptr;
		while(NCryptEnumKeys(h, reader.isEmpty() ? nullptr : LPCWSTR(scope.utf16()), &keyname, &pos, NCRYPT_SILENT_FLAG) == ERROR_SUCCESS)
		{
			NCRYPT_KEY_HANDLE key = 0;
			err = NCryptOpenKey(h, &key, keyname->pszName, keyname->dwLegacyKeySpec, NCRYPT_SILENT_FLAG);
			SslCertificate cert(QWin::prop(key, NCRYPT_CERTIFICATE_PROPERTY), QSsl::Der);
			if(!cert.isNull())
			{
				QString guid = QWin::prop(h, NCRYPT_SMARTCARD_GUID_PROPERTY).trimmed();
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
		if(provider == MS_SMART_CARD_KEY_STORAGE_PROVIDER)
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
	d->err = PinOK;
	BCRYPT_PKCS1_PADDING_INFO padInfo = { NCRYPT_SHA256_ALGORITHM };
	switch( method )
	{
	case NID_sha224: padInfo.pszAlgId = L"SHA224"; break;
	case NID_sha256: padInfo.pszAlgId = NCRYPT_SHA256_ALGORITHM; break;
	case NID_sha384: padInfo.pszAlgId = NCRYPT_SHA384_ALGORITHM; break;
	case NID_sha512: padInfo.pszAlgId = NCRYPT_SHA512_ALGORITHM; break;
	case NID_md5_sha1: //padInfo.pszAlgId = L"SHAMD5"; break;
	default: break;
	}

	DWORD size = 0;
	QByteArray res;
	NCRYPT_KEY_HANDLE k = d->key();
	QString algo(5, 0);
	SECURITY_STATUS err = NCryptGetProperty(k, NCRYPT_ALGORITHM_GROUP_PROPERTY, PBYTE(algo.data()), DWORD((algo.size() + 1) * 2), &size, 0);
	algo.resize(size/2 - 1);
	bool isRSA = algo == QStringLiteral("RSA");
	QEventLoop e;
	std::thread([&]{
		err = NCryptSignHash(k, isRSA ? &padInfo : nullptr, PBYTE(digest.constData()), DWORD(digest.size()),
			nullptr, 0, &size, isRSA ? BCRYPT_PAD_PKCS1 : 0);
		if(FAILED(err))
			return e.exit();
		res.resize(int(size));
		err = NCryptSignHash(k, isRSA ? &padInfo : nullptr, PBYTE(digest.constData()), DWORD(digest.size()),
			PBYTE(res.data()), DWORD(res.size()), &size, isRSA ? BCRYPT_PAD_PKCS1 : 0);
		e.exit();
	}).detach();
	e.exec();
	NCryptFreeObject( k );
	switch( err )
	{
	case ERROR_SUCCESS:
		d->err = PinOK;
		return res;
	case SCARD_W_CANCELLED_BY_USER:
		d->err = PinCanceled; break;
	default:
		res.clear();
		break;
	}
	return res;
}
