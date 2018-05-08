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

#include <common/QPCSC.h>
#include <common/TokenData.h>

#include <QtCore/QDebug>
#include <QtCore/QLibrary>
#include <QtCore/QStringList>
#include <QtNetwork/QSslKey>
#include <QtWidgets/QApplication>
#include <QtWidgets/QWidget>

#include <WinCrypt.h>

#include <openssl/obj_mac.h>

struct QCNGCache
{
	QString guid, provider, key;
	DWORD spec;
};

class QCNG::Private
{
public:
	void enumKeys(QHash<SslCertificate,QCNGCache> &cache, LPCWSTR provider, LPCWSTR scope = nullptr);
	NCRYPT_KEY_HANDLE key() const;
	QByteArray prop( NCRYPT_HANDLE handle, LPCWSTR param, DWORD flags = 0 ) const;

	QCNGCache selected;
	QCNG::PinStatus err;
	QHash<SslCertificate,QCNGCache> cache;
};

void QCNG::Private::enumKeys( QHash<SslCertificate,QCNGCache> &cache, LPCWSTR provider, LPCWSTR scope )
{
	QCNGCache c;
	c.provider = QString::fromWCharArray(provider);

	NCRYPT_PROV_HANDLE h = 0;
	SECURITY_STATUS err = NCryptOpenStorageProvider( &h, provider, 0 );

	NCryptKeyName *keyname = nullptr;
	PVOID pos = nullptr;
	while( NCryptEnumKeys( h, scope, &keyname, &pos, NCRYPT_SILENT_FLAG ) == ERROR_SUCCESS )
	{
		c.key = QString::fromWCharArray(keyname->pszName);
		c.spec = keyname->dwLegacyKeySpec;
		qWarning() << "key" << c.key
			<< "spec" << c.spec
			<< "alg" << QString::fromWCharArray(keyname->pszAlgid)
			<< "flags" << keyname->dwFlags;

		NCRYPT_KEY_HANDLE key = 0;
		err = NCryptOpenKey( h, &key, keyname->pszName, keyname->dwLegacyKeySpec, NCRYPT_SILENT_FLAG );
		NCryptFreeBuffer( keyname );
		keyname = nullptr;

		SslCertificate cert( prop( key, NCRYPT_CERTIFICATE_PROPERTY ), QSsl::Der );
		c.guid = prop( h, NCRYPT_SMARTCARD_GUID_PROPERTY );
		cache[cert] = c;
		NCryptFreeObject( key );
	}
	NCryptFreeObject( h );
}

NCRYPT_KEY_HANDLE QCNG::Private::key() const
{
	NCRYPT_PROV_HANDLE prov = 0;
	NCryptOpenStorageProvider( &prov, LPCWSTR(selected.provider.utf16()), 0 );
	NCRYPT_KEY_HANDLE key = 0;
	NCryptOpenKey( prov, &key, LPWSTR(selected.key.utf16()), selected.spec, 0 );
	NCryptFreeObject( prov );
	return key;
}

QByteArray QCNG::Private::prop( NCRYPT_HANDLE handle, LPCWSTR param, DWORD flags ) const
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
	QHash<SslCertificate,QCNGCache> cache;
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
				QString scope = QString(R"(\\.\%1\)").arg(reader);
				d->enumKeys( cache, names[i].pszName, LPCWSTR(scope.utf16()) );
			}
		}
		else
			d->enumKeys( cache, names[i].pszName );
	}
	NCryptFreeBuffer( names );
	d->cache = cache;
	qWarning() << "End enumerationg providers";

	QList<TokenData> result;
	for(QHash<SslCertificate,QCNGCache>::const_iterator i = cache.constBegin(); i != cache.constEnd(); ++i)
	{
		TokenData t;
		t.setCard(i.key().type() & SslCertificate::EstEidType || i.key().type() & SslCertificate::DigiIDType ?
			i.value().guid : i.key().subjectInfo(QSslCertificate::CommonName));
		t.setCert(i.key());
		result << t;
	}
	return result;
}

TokenData QCNG::selectCert( const SslCertificate &cert )
{
	qWarning() << "Select:" << cert.subjectInfo( "CN" );
	if( !d->cache.contains( cert ) )
		return TokenData();

	d->selected = d->cache[cert];
	qWarning() << "Found:" << d->selected.guid << d->selected.key;
	TokenData t;
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
	bool isRSA = algo == "RSA";
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
