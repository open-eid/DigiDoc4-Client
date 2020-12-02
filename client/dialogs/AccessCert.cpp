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

#include "AccessCert.h"

#include "Application.h"
#include "SslCertificate.h"

#include <QtCore/QDateTime>
#include <QtCore/QDir>
#include <QtCore/QFile>
#include <QtCore/QStandardPaths>
#include <QtCore/QSettings>

#ifdef Q_OS_MAC
#include <Security/Security.h>
#include <Security/SecItem.h>
#endif

class AccessCert::Private
{
public:
	QString cert, pass;
};

AccessCert::AccessCert( QWidget *parent )
	: WarningDialog({}, parent)
	, d(new Private)
{
	setCancelText(::WarningDialog::tr("CLOSE"));
#ifndef Q_OS_MAC
	d->cert = Application::confValue( Application::PKCS12Cert ).toString();
	d->pass = Application::confValue( Application::PKCS12Pass ).toString();
#endif
}

AccessCert::~AccessCert()
{
#ifndef Q_OS_MAC
	Application::setConfValue( Application::PKCS12Cert, d->cert );
	Application::setConfValue( Application::PKCS12Pass, d->pass );
#endif
	delete d;
}

QSslCertificate AccessCert::cert()
{
#ifdef Q_OS_MAC
	if(SecIdentityRef identity = SecIdentityCopyPreferred(CFSTR("ocsp.sk.ee"), nullptr, nullptr))
	{
		SecCertificateRef certref = nullptr;
		SecIdentityCopyCertificate( identity, &certref );
		CFRelease( identity );
		if( !certref )
			return QSslCertificate();

		CFDataRef certdata = SecCertificateCopyData( certref );
		CFRelease( certref );
		if( !certdata )
			return QSslCertificate();

		QSslCertificate cert(QByteArray::fromRawCFData(certdata), QSsl::Der);
		CFRelease( certdata );
		return cert;
	}
#endif
	return PKCS12Certificate::fromPath(
		Application::confValue( Application::PKCS12Cert ).toString(),
		Application::confValue( Application::PKCS12Pass ).toString() ).certificate();
}

unsigned int AccessCert::count(const QString &date) const
{
	return QByteArray::fromBase64(QSettings().value(date).toByteArray()).toUInt();
}

void AccessCert::increment()
{
	if(isDefaultCert(cert()))
	{
		QString date = "AccessCertUsage" + QDate::currentDate().toString(QStringLiteral("yyyyMM"));
		QSettings().setValue(date, QString::fromUtf8(QByteArray::number(count(date) + 1).toBase64()));
	}
}

bool AccessCert::isDefaultCert(const QSslCertificate &cert) const
{
	static const QList<QByteArray> list {
		// CN=Riigi Infos\xC3\xBCsteemi Amet, SN = da:98:09:46:6d:57:51:65:48:8b:b2:14:0d:9e:19:27
		QByteArray::fromHex("aa8ee5735ec72d411bc88d39dec0b3648b1b4c81")
	};
	return list.contains(cert.digest(QCryptographicHash::Sha1));
}

bool AccessCert::installCert( const QByteArray &data, const QString &password )
{
#ifdef Q_OS_MAC
	CFDataRef pkcs12data = data.toRawCFData();
	SecExternalFormat format = kSecFormatPKCS12;
	SecExternalItemType type = kSecItemTypeAggregate;

	SecItemImportExportKeyParameters params = {};
	params.version = SEC_KEY_IMPORT_EXPORT_PARAMS_VERSION;
	params.flags = kSecKeyImportOnlyOne|kSecKeyNoAccessControl;
	CFTypeRef keyAttributes[] = { kSecAttrIsPermanent, kSecAttrIsExtractable };
	params.keyAttributes = CFArrayCreate(nullptr,
		(const void **)keyAttributes, sizeof(keyAttributes) / sizeof(keyAttributes[0]), nullptr);
	CFTypeRef keyUsage[] = { kSecAttrCanDecrypt, kSecAttrCanUnwrap, kSecAttrCanDerive };
	params.keyUsage = CFArrayCreate(nullptr,
		(const void **)keyUsage, sizeof(keyUsage) / sizeof(keyUsage[0]), nullptr);
	params.passphrase = password.toCFString();

	SecKeychainRef keychain;
	SecKeychainCopyDefault( &keychain );
	CFArrayRef items = nullptr;
	OSStatus err = SecItemImport(pkcs12data, nullptr, &format, &type, 0, &params, keychain, &items);
	CFRelease( pkcs12data );
	CFRelease( params.passphrase );
	CFRelease( params.keyUsage );
	CFRelease( params.keyAttributes );

	if( err != errSecSuccess )
	{
		showWarning( tr("Failed to save server access certificate file to KeyChain!") );
		return false;
	}

	SecIdentityRef identity = nullptr;
	for( CFIndex i = 0; i < CFArrayGetCount( items ); ++i )
	{
		CFTypeRef item = CFTypeRef(CFArrayGetValueAtIndex( items, i ));
		if( CFGetTypeID( item ) == SecIdentityGetTypeID() )
			identity = SecIdentityRef(item);
	}

	err = SecIdentitySetPreferred(identity, CFSTR("ocsp.sk.ee"), nullptr);
	CFRelease( items );
	if( err != errSecSuccess )
	{
		showWarning( tr("Failed to save server access certificate file to KeyChain!") );
		return false;
	}
#else
	SslCertificate cert(PKCS12Certificate(data, password).certificate());
	if(cert.isNull())
		return false;
	QString path = QStandardPaths::writableLocation(QStandardPaths::DataLocation);
	if ( !QDir( path ).exists() )
		QDir().mkpath( path );

	QFile f(QStringLiteral("%1/%2.p12").arg(path, cert.subjectInfo("serialNumber")));
	if ( !f.open( QIODevice::WriteOnly|QIODevice::Truncate ) )
	{
		showWarning( tr("Failed to save server access certificate file to %1!\n%2")
			.arg( f.fileName() )
			.arg( f.errorString() ) );
		return false;
	}
	f.write( data );
	f.close();

	Application::setConfValue( Application::PKCS12Cert, d->cert = f.fileName() );
	Application::setConfValue( Application::PKCS12Pass, d->pass = password );
#endif
	return true;
}

void AccessCert::remove()
{
#ifdef Q_OS_MAC
	SecIdentityRef identity = SecIdentityCopyPreferred(CFSTR("ocsp.sk.ee"), nullptr, nullptr);
	if( !identity )
		return;

	SecCertificateRef certref = nullptr;
	SecKeyRef keyref = nullptr;
	OSStatus err = 0;
	err = SecIdentityCopyCertificate( identity, &certref );
	err = SecIdentityCopyPrivateKey( identity, &keyref );
	CFRelease( identity );
	err = SecKeychainItemDelete( SecKeychainItemRef(certref) );
	err = SecKeychainItemDelete( SecKeychainItemRef(keyref) );
	CFRelease( certref );
	CFRelease( keyref );
#else
	Application::clearConfValue( Application::PKCS12Cert );
	Application::clearConfValue( Application::PKCS12Pass );
	d->cert = Application::confValue( Application::PKCS12Cert ).toString();
	d->pass = Application::confValue( Application::PKCS12Pass ).toString();
#endif
}

void AccessCert::showWarning( const QString &msg )
{
	setText( msg );
	exec();
}

bool AccessCert::validate()
{
	QString date = "AccessCertUsage" + QDate::currentDate().toString(QStringLiteral("yyyyMM"));
	QSettings s;
	if(s.value(date).isNull())
	{
		for(const QString &key: s.allKeys())
			if(key.startsWith(QStringLiteral("AccessCertUsage")))
				s.remove(key);
	}

	if(Application::confValue(Application::PKCS12Disable, false).toBool())
		return true;

	SslCertificate c = cert();
	if(isDefaultCert(c) && (!c.isValid() || c.expiryDate() < QDateTime::currentDateTime().addDays(8)))
	{
		showWarning(tr(
			"Server access certificate expired on %1. To renew the certificate please update your ID "
			"software, which you can get from <a href=\"https://www.id.ee/en/\">www.id.ee</a>. "
			"Additional info is available ID-helpline (+372) 666 8888.")
			.arg(c.expiryDate().toLocalTime().toString(QStringLiteral("dd.MM.yyyy"))));
		if(!c.isValid())
			return false;
	}
	else if(!c.isValid())
	{
		showWarning(tr(
			"Server access certificate expired on %1. To renew the certificate please "
			"contact IT support team of your company. Additional information is available "
			"<a href=\"mailto:sales@sk.ee\">sales@sk.ee</a> or phone (+372) 610 1885")
			.arg(c.expiryDate().toLocalTime().toString(QStringLiteral("dd.MM.yyyy"))));
		return false;
	}
	else if(c.expiryDate() < QDateTime::currentDateTime().addDays(8))
	{
		showWarning(tr(
			"Server access certificate is about to expire on %1. To renew the certificate "
			"please contact IT support team of your company. Additional information is available "
			"<a href=\"mailto:sales@sk.ee\">sales@sk.ee</a> or phone (+372) 610 1885")
			.arg(c.expiryDate().toLocalTime().toString(QStringLiteral("dd.MM.yyyy"))));
	}
	if(isDefaultCert(c) && count(date) >= 50)
	{
		showWarning(tr(
			"The limit of free digital signatures per month is about to exceed. To create more "
			"digital signatures you need to conclude a contract with a service provider. Read more "
			"<a href=\"https://www.id.ee/en/article/for-organisations-that-sign-large-quantities-of-documents-using-digidoc4-client/\">here</a>"));
	}
	return true;
}
