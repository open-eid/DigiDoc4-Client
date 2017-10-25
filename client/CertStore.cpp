/*
 * QEstEidUtil
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

#include "CertStore.h"

#include <common/SslCertificate.h>

#include <QtCore/QtEndian>
#include <QtNetwork/QSslKey>

#include <qt_windows.h>
#include <WinCrypt.h>
#include <ncrypt.h>

class CertStorePrivate
{
public:
	CertStorePrivate(): s(0) {}

	PCCERT_CONTEXT certContext( const QSslCertificate &cert ) const
	{
		QByteArray data = cert.toDer();
		return CertCreateCertificateContext( X509_ASN_ENCODING,
			(const PBYTE)data.constData(), data.size() );
	}

	HCERTSTORE s;
};

CertStore::CertStore()
: d( new CertStorePrivate )
{
	d->s = CertOpenStore( CERT_STORE_PROV_SYSTEM_W,
		X509_ASN_ENCODING, 0, CERT_SYSTEM_STORE_CURRENT_USER, L"MY" );
}

CertStore::~CertStore()
{
	CertCloseStore( d->s, 0 );
	delete d;
}

bool CertStore::add( const QSslCertificate &cert, const QString &card )
{
	if( !d->s )
		return false;

	SslCertificate c( cert );
	DWORD keyCode = c.keyUsage().contains( SslCertificate::NonRepudiation ) ? AT_SIGNATURE : AT_KEYEXCHANGE;

	PCCERT_CONTEXT context = d->certContext( cert );
	if(!context)
		return false;

	auto _ntohs = [](quint16 x) {
		return QSysInfo::ByteOrder == QSysInfo::BigEndian ? x : qbswap<quint16>(x);
	};
	auto _ntohl = [](quint32 x) {
		return QSysInfo::ByteOrder == QSysInfo::BigEndian ? x : qbswap<quint32>(x);
	};

	DWORD size = 20;
	QByteArray hash(int(size), 0);
	CertGetCertificateContextProperty(context, CERT_SHA1_HASH_PROP_ID, hash.data(), &size);
	GUID g;
	// convert UUID to local byte order
	memcpy(&g, hash.data(), sizeof(g));
	g.Data1 = _ntohl(g.Data1);
	g.Data2 = _ntohs(g.Data2);
	g.Data3 = _ntohs(g.Data3);

	// put in the variant and version bits
	g.Data3 &= 0xFFF;
	g.Data3 |= (5 << 12);
	g.Data4[0] &= 0x3F;
	g.Data4[0] |= 0x80;

	std::wstring guid(40, 0);
	swprintf(&guid[0], sizeof(guid) / 2, L"%8.8x-%4.4x-%4.4x-%2.2x%2.2x-%2.2x%2.2x%2.2x%2.2x%2.2x%2.2x",
		g.Data1, g.Data2, g.Data3, g.Data4[0], g.Data4[1], g.Data4[2],
		g.Data4[3], g.Data4[4], g.Data4[5], g.Data4[6], g.Data4[7]);

	QString str = QString( "%1 %2" )
		.arg( keyCode == AT_SIGNATURE ? "Signature" : "Authentication" )
		.arg( c.toString( "SN, GN" ).toUpper() );
	CRYPT_DATA_BLOB DataBlob = { (str.length() + 1) * sizeof(QChar), (BYTE*)str.utf16() };
	CertSetCertificateContextProperty( context, CERT_FRIENDLY_NAME_PROP_ID, 0, &DataBlob );

	CRYPT_KEY_PROV_INFO RSAKeyProvInfo  =
	{ &guid[0], L"Microsoft Base Smart Card Crypto Provider", PROV_RSA_FULL, 0, 0, 0, keyCode };
	CRYPT_KEY_PROV_INFO ECKeyProvInfo =  
	{ &guid[0], L"Microsoft Smart Card Key Storage Provider", 0, 0, 0, 0, 0 }; 
	CertSetCertificateContextProperty(context, CERT_KEY_PROV_INFO_PROP_ID, 0, cert.publicKey().algorithm() == QSsl::Rsa ? &RSAKeyProvInfo : &ECKeyProvInfo);  

	bool result = CertAddCertificateContextToStore( d->s, context, CERT_STORE_ADD_REPLACE_EXISTING, 0 );
	CertFreeCertificateContext( context );
	return result;
}

bool CertStore::find( const QSslCertificate &cert ) const
{
	if( !d->s )
		return false;

	PCCERT_CONTEXT context = d->certContext( cert );
	if( !context )
		return false;
	PCCERT_CONTEXT result = CertFindCertificateInStore( d->s, X509_ASN_ENCODING,
		0, CERT_FIND_SUBJECT_CERT, context->pCertInfo, 0 );
	CertFreeCertificateContext( context );
	return result;
}
QList<QSslCertificate> CertStore::list() const
{
	QList<QSslCertificate> list;
	PCCERT_CONTEXT c = 0;
	while( (c = CertEnumCertificatesInStore( d->s, c )) )
		list << QSslCertificate( QByteArray( (char*)c->pbCertEncoded, c->cbCertEncoded ), QSsl::Der );
	CertFreeCertificateContext( c );
	return list;
}

bool CertStore::remove( const QSslCertificate &cert )
{
	if( !d->s )
		return false;

	PCCERT_CONTEXT context = d->certContext( cert );
	if( !context )
		return false;
	bool result = false;
	PCCERT_CONTEXT scontext = 0;
	while( (scontext = CertEnumCertificatesInStore( d->s, scontext )) )
	{
		if( CertCompareCertificate( X509_ASN_ENCODING,
				context->pCertInfo, scontext->pCertInfo ) )
			result = CertDeleteCertificateFromStore( CertDuplicateCertificateContext( scontext ) );
	}
	CertFreeCertificateContext( context );
	return result;
}
