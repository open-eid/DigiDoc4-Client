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

#include <common/QPCSC.h>
#include <common/TokenData.h>

#include <QtCore/QDebug>
#include <QtCore/QLibrary>
#include <QtCore/QStringList>
#include <QtNetwork/QSslKey>
#include <QtWidgets/QApplication>
#include <QtWidgets/QWidget>

#include <wincrypt.h>

#include <openssl/obj_mac.h>

class QCNG::Private
{
public:
	struct Cache
	{
		QString provider, key, guid, reader;
		DWORD spec;
	};

	NCRYPT_KEY_HANDLE key() const
	{
		NCRYPT_PROV_HANDLE prov = 0;
		NCryptOpenStorageProvider( &prov, LPCWSTR(selected.provider.utf16()), 0 );
		NCRYPT_KEY_HANDLE key = 0;
		NCryptOpenKey( prov, &key, LPWSTR(selected.key.utf16()), selected.spec, 0 );
		NCryptFreeObject( prov );
		return key;
	}

	Cache selected;
	QCNG::PinStatus err;
	QHash<SslCertificate,Cache> cache;
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

QByteArray QCNG::decrypt( const QByteArray &data )
{
	d->err = PinUnknown;
	DWORD size = 256;
	QByteArray res(int(size), 0);
	NCRYPT_KEY_HANDLE k = d->key();
	SECURITY_STATUS err = NCryptDecrypt(k, PBYTE(data.constData()), DWORD(data.size()), nullptr,
		PBYTE(res.data()), DWORD(res.size()), &size, NCRYPT_PAD_PKCS1_FLAG);
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
	d->err = PinUnknown;
	QByteArray derived;
	NCRYPT_PROV_HANDLE prov = 0;
	if(NCryptOpenStorageProvider(&prov, LPCWSTR(d->selected.provider.utf16()), 0))
		return derived;
	NCRYPT_KEY_HANDLE key = 0;
	if(NCryptOpenKey(prov, &key, LPWSTR(d->selected.key.utf16()), d->selected.spec, 0))
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
	qWarning() << "Start enumerationg providers";
	QHash<SslCertificate,Private::Cache> cache;
	auto enumKeys = [](QHash<SslCertificate,Private::Cache> &cache, LPCWSTR provider, const QString &reader = QString()) {
		QString scope = QStringLiteral(R"(\\.\%1\)").arg(reader);
		NCRYPT_PROV_HANDLE h = 0;
		SECURITY_STATUS err = NCryptOpenStorageProvider(&h, provider, 0);
		NCryptKeyName *keyname = nullptr;
		PVOID pos = nullptr;
		while(NCryptEnumKeys(h, LPCWSTR(scope.utf16()), &keyname, &pos, NCRYPT_SILENT_FLAG) == ERROR_SUCCESS)
		{
			NCRYPT_KEY_HANDLE key = 0;
			err = NCryptOpenKey(h, &key, keyname->pszName, keyname->dwLegacyKeySpec, NCRYPT_SILENT_FLAG);
			SslCertificate cert(QWin::prop(key, NCRYPT_CERTIFICATE_PROPERTY), QSsl::Der);
			Private::Cache c = {
				QString::fromWCharArray(provider),
				QString::fromWCharArray(keyname->pszName),
				QWin::prop(h, NCRYPT_SMARTCARD_GUID_PROPERTY).trimmed(),
				reader,
				keyname->dwLegacyKeySpec
			};
			qWarning() << "key" << c.key
				<< "spec" << c.spec
				<< "alg" << QString::fromWCharArray(keyname->pszAlgid)
				<< "flags" << keyname->dwFlags;
			cache[cert] = c;
			NCryptFreeObject(key);
			NCryptFreeBuffer(keyname);
			keyname = nullptr;
		}
		NCryptFreeObject(h);
	};

	DWORD count = 0;
	NCryptProviderName *names = nullptr;
	NCryptEnumStorageProviders( &count, &names, NCRYPT_SILENT_FLAG );
	for( DWORD i = 0; i < count; ++i )
	{
		qWarning() << "Found provider" << QString::fromWCharArray(names[i].pszName);
		if( wcscmp( names[i].pszName, MS_SMART_CARD_KEY_STORAGE_PROVIDER ) == 0 )
		{
			for( const QString &reader: QPCSC::instance().readers() )
			{
				qWarning() << reader;
				enumKeys(cache, names[i].pszName, reader);
			}
		}
		else
			enumKeys(cache, names[i].pszName);
	}
	NCryptFreeBuffer( names );
	d->cache = cache;
	qWarning() << "End enumerationg providers";

	QList<TokenData> result;
	for(QHash<SslCertificate,Private::Cache>::const_iterator i = cache.constBegin(); i != cache.constEnd(); ++i)
	{
		TokenData t;
		t.setReaders({i.value().reader});
		t.setCard(i.key().type() & SslCertificate::EstEidType || i.key().type() & SslCertificate::DigiIDType ?
			i.value().guid : i.key().subjectInfo(QSslCertificate::CommonName) + "-" + i.key().serialNumber());
		t.setCert(i.key());
		result << t;
	}
	return result;
}

TokenData QCNG::selectCert( const SslCertificate &cert )
{
	qWarning() << "Select:" << cert.subjectInfo( "CN" );
	TokenData t;
	if( !d->cache.contains( cert ) )
		return t;

	d->selected = d->cache[cert];
	qWarning() << "Found:" << d->selected.guid << d->selected.key;
	t.setCard( cert.type() & SslCertificate::EstEidType || cert.type() & SslCertificate::DigiIDType ?
		d->selected.guid : cert.subjectInfo( QSslCertificate::CommonName ) );
	t.setCert( cert );

	return t;
}

QByteArray QCNG::sign( int method, const QByteArray &digest ) const
{
	d->err = PinUnknown;
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
	err = NCryptSignHash(k, isRSA ? &padInfo : nullptr, PBYTE(digest.constData()), DWORD(digest.size()),
		nullptr, 0, &size, isRSA ? BCRYPT_PAD_PKCS1 : 0);
	if(FAILED(err))
		return res;
	res.resize(int(size));
	err = NCryptSignHash(k, isRSA ? &padInfo : nullptr, PBYTE(digest.constData()), DWORD(digest.size()),
		PBYTE(res.data()), DWORD(res.size()), &size, isRSA ? BCRYPT_PAD_PKCS1 : 0);
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
