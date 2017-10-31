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

#include "QPKCS11_p.h"

#include "dialogs/PinPopup.h"
#include <common/QPCSC.h>
#include <common/Settings.h>

#include <QtCore/QDebug>
#include <QtCore/QEventLoop>
#include <QtCore/QFile>
#include <QtCore/QHash>
#include <QtCore/QMutex>
#include <QtCore/QStringList>
#include <QtWidgets/QApplication>

#include <openssl/obj_mac.h>

#define toQByteArray(X) QByteArray::fromRawData((const char*)X, sizeof(X)).toUpper()

QByteArray QPKCS11Private::attribute( CK_SESSION_HANDLE session, CK_OBJECT_HANDLE obj, CK_ATTRIBUTE_TYPE type ) const
{
	if(!f)
		return QByteArray();
	CK_ATTRIBUTE attr = { type, 0, 0 };
	if( f->C_GetAttributeValue( session, obj, &attr, 1 ) != CKR_OK )
		return QByteArray();
	QByteArray data( attr.ulValueLen, 0 );
	attr.pValue = data.data();
	if( f->C_GetAttributeValue( session, obj, &attr, 1 ) != CKR_OK )
		return QByteArray();
	return data;
}

QVector<CK_OBJECT_HANDLE> QPKCS11Private::findObject(CK_SESSION_HANDLE session, CK_OBJECT_CLASS cls, const QByteArray &id) const
{
	if(!f)
		return QVector<CK_OBJECT_HANDLE>();
	QVector<CK_ATTRIBUTE> attr{ { CKA_CLASS, &cls, sizeof(cls) } };
	if(!id.isEmpty())
		attr.append({ CKA_ID, (void*)id.data(), (unsigned long)id.size() });
	if(f->C_FindObjectsInit(session, attr.data(), attr.size()) != CKR_OK)
		return QVector<CK_OBJECT_HANDLE>();

	CK_ULONG count = 32;
	QVector<CK_OBJECT_HANDLE> result( count );
	CK_RV err = f->C_FindObjects( session, result.data(), result.count(), &count );
	result.resize( count );
	f->C_FindObjectsFinal( session );
	return err == CKR_OK ? result : QVector<CK_OBJECT_HANDLE>();
}

void QPKCS11Private::run()
{
	result = f->C_Login( session, CKU_USER, 0, 0 );
}

QVector<CK_SLOT_ID> QPKCS11Private::slotIds( bool token_present ) const
{
	if(!f)
		return QVector<CK_SLOT_ID>();
	CK_ULONG size = 0;
	if( f->C_GetSlotList( token_present, 0, &size ) != CKR_OK )
		return QVector<CK_SLOT_ID>();
	QVector<CK_SLOT_ID> result( size );
	if( f->C_GetSlotList( token_present, result.data(), &size ) == CKR_OK )
		result.resize( size );
	else
		result.clear();
	return result;
}

void QPKCS11Private::updateTokenFlags( TokenData &t, CK_ULONG f ) const
{
	t.setFlag( TokenData::PinCountLow, f & CKF_SO_PIN_COUNT_LOW || f & CKF_USER_PIN_COUNT_LOW );
	t.setFlag( TokenData::PinFinalTry, f & CKF_SO_PIN_FINAL_TRY || f & CKF_USER_PIN_FINAL_TRY );
	t.setFlag( TokenData::PinLocked, f & CKF_SO_PIN_LOCKED || f & CKF_USER_PIN_LOCKED );
}

CK_RV QPKCS11Private::createMutex( CK_VOID_PTR_PTR mutex )
{
	*mutex = CK_VOID_PTR(new QMutex);
	if( *mutex ) return CKR_OK;
	return CKR_HOST_MEMORY;
}

CK_RV QPKCS11Private::destroyMutex( CK_VOID_PTR mutex )
{
	delete static_cast<QMutex*>(mutex);
	return CKR_OK;
}

CK_RV QPKCS11Private::lockMutex( CK_VOID_PTR mutex )
{
	static_cast<QMutex*>( mutex )->lock();
	return CKR_OK;
}

CK_RV unlockMutex( CK_VOID_PTR mutex )
{
	static_cast<QMutex*>( mutex )->unlock();
	return CKR_OK;
}



QPKCS11::QPKCS11( QObject *parent )
:	QObject( parent )
,	d( new QPKCS11Private )
{
}

QPKCS11::~QPKCS11()
{
	logout();
	if( d->f )
		d->f->C_Finalize( 0 );
	d->lib.unload();
	delete d;
}

QByteArray QPKCS11::encrypt( const QByteArray &data ) const
{
	QVector<CK_OBJECT_HANDLE> key = d->findObject(d->session, CKO_PRIVATE_KEY, d->id);
	if(key.size() != 1)
		return QByteArray();

	CK_MECHANISM mech = { CKM_RSA_PKCS, 0, 0 };
	if(d->f->C_EncryptInit(d->session, &mech, key[0]) != CKR_OK)
		return QByteArray();

	CK_ULONG size = 0;
	if( d->f->C_Encrypt( d->session, CK_CHAR_PTR(data.constData()), data.size(), 0, &size ) != CKR_OK )
		return QByteArray();

	QByteArray result( size, 0 );
	if( d->f->C_Encrypt( d->session, CK_CHAR_PTR(data.constData()), data.size(), CK_CHAR_PTR(result.data()), &size ) != CKR_OK )
		return QByteArray();
	return result;
}

QString QPKCS11::errorString( PinStatus error )
{
	switch( error )
	{
	case QPKCS11::PinOK: return QString();
	case QPKCS11::PinCanceled: return tr("PIN Canceled");
	case QPKCS11::PinLocked: return tr("PIN locked");
	case QPKCS11::PinIncorrect: return tr("PIN Incorrect");
	case QPKCS11::GeneralError: return tr("PKCS11 general error");
	case QPKCS11::DeviceError: return tr("PKCS11 device error");
	default: return tr("PKCS11 unknown error");
	}
}

QByteArray QPKCS11::decrypt( const QByteArray &data ) const
{
	QVector<CK_OBJECT_HANDLE> key = d->findObject(d->session, CKO_PRIVATE_KEY, d->id);
	if(key.size() != 1)
		return QByteArray();

	CK_MECHANISM mech = { CKM_RSA_PKCS, 0, 0 };
	if(d->f->C_DecryptInit(d->session, &mech, key[0]) != CKR_OK)
		return QByteArray();

	CK_ULONG size = 0;
	if( d->f->C_Decrypt( d->session, CK_CHAR_PTR(data.constData()), data.size(), 0, &size ) != CKR_OK )
		return QByteArray();

	QByteArray result( size, 0 );
	if( d->f->C_Decrypt( d->session, CK_CHAR_PTR(data.constData()), data.size(), CK_CHAR_PTR(result.data()), &size ) != CKR_OK )
		return QByteArray();
	return result;
}

bool QPKCS11::isLoaded() const
{
	return d->f != 0;
}

bool QPKCS11::load( const QString &driver )
{
	qWarning() << "Loading:" << driver;
	d->lib.setFileName( driver );
	CK_C_GetFunctionList l = CK_C_GetFunctionList(d->lib.resolve( "C_GetFunctionList" ));
	if( !l || l( &d->f ) != CKR_OK )
	{
		qWarning() << "Failed to resolve symbols";
		return false;
	}

#if 0
	CK_C_INITIALIZE_ARGS init_args = {
		QPKCS11Private::createMutex,
		QPKCS11Private::destroyMutex,
		QPKCS11Private::lockMutex,
		QPKCS11Private::destroyMutex,
		0, 0 };
#else
	CK_C_INITIALIZE_ARGS init_args = { 0, 0, 0, 0, CKF_OS_LOCKING_OK, 0 };
#endif
	CK_RV err = d->f->C_Initialize( &init_args );
	if( err != CKR_OK && err != CKR_CRYPTOKI_ALREADY_INITIALIZED )
	{
		qWarning() << "Failed to initalize";
		return false;
	}

	CK_INFO info;
	d->f->C_GetInfo( &info );
	qWarning()
		<< QString("%1 (%2.%3)").arg( toQByteArray( info.manufacturerID ).constData() )
			.arg( info.cryptokiVersion.major ).arg( info.cryptokiVersion.minor ) << endl
		<< QString("%1 (%2.%3)").arg( toQByteArray( info.libraryDescription ).constData() )
			.arg( info.libraryVersion.major ).arg( info.libraryVersion.minor ) << endl
		<< "Flags:" << info.flags;
	return true;
}

QPKCS11::PinStatus QPKCS11::login( const TokenData &_t )
{
	logout();

	QVector<CK_SLOT_ID> list = d->slotIds( true );
	// Hack: Workaround broken FIN pkcs11 drivers exposing Signing certificate in slot 1
	std::reverse(list.begin(), list.end());
	for( CK_SLOT_ID slot: list )
	{
		if( d->session )
			d->f->C_CloseSession( d->session );
		d->session = 0;
		if( d->f->C_OpenSession( slot, CKF_SERIAL_SESSION, 0, 0, &d->session ) != CKR_OK )
			continue;

		for( CK_OBJECT_HANDLE obj: d->findObject( d->session, CKO_CERTIFICATE ) )
		{
			if( _t.cert() == QSslCertificate( d->attribute( d->session, obj, CKA_VALUE ), QSsl::Der ) )
			{
				d->id = d->attribute(d->session, obj, CKA_ID);
				break;
			}
		}
		if(!d->id.isEmpty())
			break;
	}

	CK_SESSION_INFO sessinfo;
	if(d->id.isEmpty() || d->f->C_GetSessionInfo(d->session, &sessinfo) != CKR_OK)
		return UnknownError;

	CK_TOKEN_INFO token;
	if( d->f->C_GetTokenInfo( sessinfo.slotID, &token ) != CKR_OK )
		return UnknownError;

	if( !(token.flags & CKF_LOGIN_REQUIRED) )
		return PinOK;

	TokenData t = _t;
	d->updateTokenFlags( t, token.flags );
	if( t.flags() & TokenData::PinLocked )
		return PinLocked;

	CK_RV err = CKR_OK;
	bool pin2 = SslCertificate( t.cert() ).keyUsage().keys().contains( SslCertificate::NonRepudiation );
	if( token.flags & CKF_PROTECTED_AUTHENTICATION_PATH )
	{
		PinPopup p( pin2 ? PinDialog::Pin2PinpadType : PinDialog::Pin1PinpadType, t, qApp->activeWindow() );
		connect( d, &QPKCS11Private::started, &p, &PinPopup::startTimer );
		p.open();

		QEventLoop e;
		connect( d, &QPKCS11Private::finished, &e, &QEventLoop::quit );
		d->start();
		e.exec();
		err = d->result;
	}
	else
	{
		PinPopup p( pin2 ? PinDialog::Pin2Type : PinDialog::Pin1Type, t, qApp->activeWindow() );
		if( !p.exec() )
			return PinCanceled;
		QByteArray pin = p.text().toUtf8();
		err = d->f->C_Login( d->session, CKU_USER, CK_CHAR_PTR(pin.constData()), pin.size() );
	}

	if( d->f->C_GetTokenInfo( sessinfo.slotID, &token ) == CKR_OK )
		d->updateTokenFlags( t, token.flags );

	switch( err )
	{
	case CKR_OK:
	case CKR_USER_ALREADY_LOGGED_IN:
		return PinOK;
	case CKR_CANCEL:
	case CKR_FUNCTION_CANCELED: return PinCanceled;
	case CKR_PIN_INCORRECT: return t.flags() & TokenData::PinLocked ? PinLocked : PinIncorrect;
	case CKR_PIN_LOCKED: return PinLocked;
	case CKR_DEVICE_ERROR: return DeviceError;
	case CKR_GENERAL_ERROR: return GeneralError;
	default: return UnknownError;
	}
}

void QPKCS11::logout()
{
	d->id.clear();
	if( d->f && d->session )
	{
		d->f->C_Logout( d->session );
		d->f->C_CloseSession( d->session );
	}
	d->session = 0;
}

QStringList QPKCS11::readers() const
{
	QStringList readers;
	for( CK_SLOT_ID slot: d->slotIds( false ) )
	{
		CK_SLOT_INFO info;
		d->f->C_GetSlotInfo( slot, &info );
		QByteArray man = toQByteArray( info.manufacturerID ).trimmed();
		QByteArray desc = toQByteArray( info.slotDescription ).trimmed();
		if( man.contains( "OpenSC" ) && desc.contains( "Virtual" ) )
			continue;
		readers << desc;
	}
	readers.removeDuplicates();
	return readers;
}

QList<TokenData> QPKCS11::tokens() const
{
	QList<TokenData> list;
	for( CK_SLOT_ID slot: d->slotIds( true ) )
	{
		CK_TOKEN_INFO token;
		if( d->f->C_GetTokenInfo( slot, &token ) != CKR_OK )
			continue;

		QString card = toQByteArray( token.serialNumber ).trimmed();

		CK_SESSION_HANDLE session = 0;
		if( d->f->C_OpenSession( slot, CKF_SERIAL_SESSION, 0, 0, &session ) != CKR_OK )
			continue;

		for( CK_OBJECT_HANDLE obj: d->findObject( session, CKO_CERTIFICATE ) )
		{
			SslCertificate cert( d->attribute( session, obj, CKA_VALUE ), QSsl::Der );
			if( cert.isCA() )
				continue;
			TokenData t;
			t.setCard( card );
			t.setCert( cert );
			d->updateTokenFlags( t, token.flags );
			list << t;
		}
		d->f->C_CloseSession( session );
	}
	return list;
}

QByteArray QPKCS11::sign( int type, const QByteArray &digest ) const
{
	QVector<CK_OBJECT_HANDLE> key = d->findObject(d->session, CKO_PRIVATE_KEY, d->id);
	if(key.size() != 1)
		return QByteArray();

	CK_KEY_TYPE keyType = CKK_RSA;
	CK_ATTRIBUTE attribute = { CKA_KEY_TYPE, &keyType, sizeof(keyType) };
	d->f->C_GetAttributeValue(d->session, key[0], &attribute, 1);

	CK_MECHANISM mech = { keyType == CKK_ECDSA ? CKM_ECDSA : CKM_RSA_PKCS, 0, 0 };
	if(d->f->C_SignInit(d->session, &mech, key[0]) != CKR_OK)
		return QByteArray();

	QByteArray data;
	if(keyType == CKK_RSA)
	{
		switch(type)
		{
		case NID_sha224: data += QByteArray::fromHex("302d300d06096086480165030402040500041c"); break;
		case NID_sha256: data += QByteArray::fromHex("3031300d060960864801650304020105000420"); break;
		case NID_sha384: data += QByteArray::fromHex("3041300d060960864801650304020205000430"); break;
		case NID_sha512: data += QByteArray::fromHex("3051300d060960864801650304020305000440"); break;
		default: break;
		}
	}
	data.append(digest);

	CK_ULONG size = 0;
	if( d->f->C_Sign( d->session, CK_CHAR_PTR(data.constData()),
			data.size(), 0, &size ) != CKR_OK )
		return QByteArray();

	QByteArray sig( size, 0 );
	if( d->f->C_Sign( d->session, CK_CHAR_PTR(data.constData()),
			data.size(), CK_CHAR_PTR(sig.data()), &size ) != CKR_OK )
		return QByteArray();
	return sig;
}

bool QPKCS11::verify( const QByteArray &data, const QByteArray &signature ) const
{
	QVector<CK_OBJECT_HANDLE> key = d->findObject(d->session, CKO_PRIVATE_KEY, d->id);
	if(key.size() != 1)
		return false;

	CK_MECHANISM mech = { CKM_RSA_PKCS, 0, 0 };
	if(d->f->C_VerifyInit(d->session, &mech, key[0]) != CKR_OK)
		return false;

	return d->f->C_Verify( d->session, CK_CHAR_PTR(data.constData()),
		data.size(), CK_CHAR_PTR(signature.data()), signature.size() ) == CKR_OK;
}



class QPKCS11StackPrivate
{
public:
	QMultiHash<QString,QByteArray> drivers;
	QPKCS11 *active = nullptr;
	QString activeDriver;
};

QPKCS11Stack::QPKCS11Stack( QObject *parent )
:	QObject( parent )
,	d( new QPKCS11StackPrivate )
{
}

bool QPKCS11Stack::isLoaded() const
{
	return d->active && d->active->isLoaded();
}

QPKCS11Stack::~QPKCS11Stack()
{
	delete d->active;
	delete d;
}

QByteArray QPKCS11Stack::encrypt(const QByteArray &data) const
{
	return d->active ? d->active->encrypt(data) : QByteArray();
}

QByteArray QPKCS11Stack::decrypt(const QByteArray &data) const
{
	return d->active ? d->active->decrypt(data) : QByteArray();
}

bool QPKCS11Stack::load(const QString &defaultDriver)
{
	d->drivers.insert(defaultDriver, QByteArray());
#ifdef Q_OS_MAC
	d->drivers.insert("/Library/latvia-eid/lib/otlv-pkcs11.so", "3BDD18008131FE45904C41545649412D65494490008C");
	d->drivers.insert("/Library/Security/tokend/CCSuite.tokend/Contents/Frameworks/libccpkip11.dylib", "3BF81300008131FE45536D617274417070F8");
	d->drivers.insert("/Library/Security/tokend/CCSuite.tokend/Contents/Frameworks/libccpkip11.dylib", "3B7D94000080318065B08311C0A983009000");
	if(QFile::exists("/Library/mPolluxDigiSign/libcryptoki.dylib"))
		d->drivers.insert("/Library/mPolluxDigiSign/libcryptoki.dylib", "3B7B940000806212515646696E454944");
	else
		d->drivers.insert("/Library/OpenSC/lib/opensc-pkcs11.so", "3B7B940000806212515646696E454944");
	d->drivers.insert("/Library/Frameworks/eToken.framework/Versions/Current/libeToken.dylib", "3BD5180081313A7D8073C8211030");
	d->drivers.insert("/Library/Frameworks/eToken.framework/Versions/Current/libeToken.dylib", "3BD518008131FE7D8073C82110F4");
	QVariantMap PKCS11 = Settings().value("PKCS11").toMap();
	for(auto it = PKCS11.cbegin(), end = PKCS11.cend(); it != end; ++it)
		d->drivers.insert(it.value().toString(), it.key().toLocal8Bit());
#elif defined(Q_OS_WIN)
	d->drivers.insert("OTLvP11.dll", "3BDD18008131FE45904C41545649412D65494490008C");
	d->drivers.insert(qApp->applicationDirPath() + "/../CryptoTech/CryptoCard/CCPkiP11.dll", "3BF81300008131FE45536D617274417070F8");
	d->drivers.insert(qApp->applicationDirPath() + "/../CryptoTech/CryptoCard/CCPkiP11.dll", "3B7D94000080318065B08311C0A983009000");
#else
	d->drivers.insert("otlv-pkcs11.so", "3BDD18008131FE45904C41545649412D65494490008C");
	d->drivers.insert("/usr/lib/ccs/libccpkip11.so", "3BF81300008131FE45536D617274417070F8");
	d->drivers.insert("/usr/lib/ccs/libccpkip11.so", "3B7D94000080318065B08311C0A983009000");
#endif
	updateDrivers();
	return isLoaded();
}

void QPKCS11Stack::loadDriver(const QString &driver) const
{
	if(!driver.isEmpty() && d->activeDriver == driver)
		return;
	if(driver.isEmpty() && d->activeDriver == d->drivers.key(""))
		return;
	delete d->active;
	d->activeDriver = driver.isEmpty() ? d->drivers.key("") : driver;
	d->active = new QPKCS11();
	d->active->load( d->activeDriver );
}

QStringList QPKCS11Stack::readers() const
{
	return QPCSC::instance().readers();
}

QList<TokenData> QPKCS11Stack::tokens() const
{
	QList<TokenData> list;
	updateDrivers();
	if(d->active)
		list << d->active->tokens();
	return list;
}

QPKCS11::PinStatus QPKCS11Stack::login(const TokenData &t)
{
	return d->active ? d->active->login(t) : QPKCS11::UnknownError;
}

void QPKCS11Stack::logout()
{
	if( d->active )
		d->active->logout();
}

QByteArray QPKCS11Stack::sign(int type, const QByteArray &digest) const
{
	return d->active ? d->active->sign(type, digest) : QByteArray();
}

void QPKCS11Stack::updateDrivers() const
{
	QList<QByteArray> atrs;
	for(const QString &reader: QPCSC::instance().readers())
	{
		QPCSCReader r(reader, &QPCSC::instance());
		if(r.isPresent())
			atrs << r.atr();
	}
	for(const QByteArray &atr: atrs)
	{
		QString driver = d->drivers.key(atr);
		if(!driver.isEmpty())
			return loadDriver(driver);
	}
	loadDriver(QString());
}

bool QPKCS11Stack::verify(const QByteArray &data, const QByteArray &signature) const
{
	return d->active ? d->active->verify(data, signature) : false;
}
