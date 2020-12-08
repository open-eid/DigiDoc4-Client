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

#include "SslCertificate.h"
#include "TokenData.h"
#include "dialogs/PinPopup.h"

#include <common/QPCSC.h>
#include <crypto/CryptoDoc.h>

#include <QtCore/QDebug>
#include <QtWidgets/QApplication>

#include <openssl/obj_mac.h>

#define toQByteArray(X) QByteArray::fromRawData((const char*)(X), sizeof(X)).toUpper()

QWidget* rootWindow()
{
	QWidget* win = qApp->activeWindow();
	QWidget* root = nullptr;

	if (!win)
	{
		for (QWidget *widget: qApp->topLevelWidgets())
		{
			if (widget->isWindow())
			{
				win = widget;
				break;
			}
		}
	}

	while(win)
	{
		root = win;
		win = win->parentWidget();
	}

	return root;
}

QByteArray QPKCS11::Private::attribute(CK_SESSION_HANDLE session, CK_OBJECT_HANDLE obj, CK_ATTRIBUTE_TYPE type) const
{
	QByteArray data;
	if(!f)
		return data;
	CK_ATTRIBUTE attr = { type, nullptr, 0 };
	if( f->C_GetAttributeValue( session, obj, &attr, 1 ) != CKR_OK )
		return QByteArray();
	data.resize(int(attr.ulValueLen));
	attr.pValue = data.data();
	if( f->C_GetAttributeValue( session, obj, &attr, 1 ) != CKR_OK )
		data.clear();
	return data;
}

std::vector<CK_OBJECT_HANDLE> QPKCS11::Private::findObject(CK_SESSION_HANDLE session, CK_OBJECT_CLASS cls, const QByteArray &id) const
{
	std::vector<CK_OBJECT_HANDLE> result;
	if(!f)
		return result;
	CK_BBOOL _true = CK_TRUE;
	std::vector<CK_ATTRIBUTE> attr{
		{ CKA_CLASS, &cls, sizeof(cls) },
		{ CKA_TOKEN, &_true, sizeof(_true) }
	};
	if(!id.isEmpty())
		attr.push_back({ CKA_ID, CK_VOID_PTR(id.data()), CK_ULONG(id.size()) });
	if(f->C_FindObjectsInit(session, attr.data(), CK_ULONG(attr.size())) != CKR_OK)
		return result;

	CK_ULONG count = 32;
	result.resize(size_t(count));
	if(f->C_FindObjects(session, result.data(), CK_ULONG(result.size()), &count) == CKR_OK)
		result.resize(size_t(count));
	else
		result.clear();
	f->C_FindObjectsFinal( session );
	return result;
}

void QPKCS11::Private::run()
{
	result = f->C_Login(session, CKU_USER, nullptr, 0);
}

std::vector<CK_SLOT_ID> QPKCS11::Private::slotIds(bool token_present) const
{
	std::vector<CK_SLOT_ID> result;
	if(!f)
		return result;
	CK_ULONG size = 0;
	if(f->C_GetSlotList(token_present, nullptr, &size) != CKR_OK)
		return result;
	result.resize(size_t(size));
	if( f->C_GetSlotList( token_present, result.data(), &size ) == CKR_OK )
		result.resize(size_t(size));
	else
		result.clear();
	return result;
}



QPKCS11::QPKCS11( QObject *parent )
	: QCryptoBackend(parent)
	, d(new Private)
{
}

QPKCS11::~QPKCS11()
{
	unload();
	delete d;
}

QByteArray QPKCS11::derive(const QByteArray &publicKey) const
{
	std::vector<CK_OBJECT_HANDLE> key = d->findObject(d->session, CKO_PRIVATE_KEY, d->id);
	if(key.size() != 1)
		return QByteArray();

	CK_ECDH1_DERIVE_PARAMS ecdh_parms = { CKD_NULL, 0, nullptr, CK_ULONG(publicKey.size()), CK_BYTE_PTR(publicKey.data()) };
	CK_MECHANISM mech = { CKM_ECDH1_DERIVE, &ecdh_parms, sizeof(CK_ECDH1_DERIVE_PARAMS) };
	CK_BBOOL _false = FALSE;
	CK_OBJECT_CLASS newkey_class = CKO_SECRET_KEY;
	CK_KEY_TYPE newkey_type = CKK_GENERIC_SECRET;
	std::vector<CK_ATTRIBUTE> newkey_template{
		{CKA_TOKEN, &_false, sizeof(_false)},
		{CKA_CLASS, &newkey_class, sizeof(newkey_class)},
		{CKA_KEY_TYPE, &newkey_type, sizeof(newkey_type)},
	};
	CK_OBJECT_HANDLE newkey = CK_INVALID_HANDLE;
	if(d->f->C_DeriveKey(d->session, &mech, key[0], newkey_template.data(), CK_ULONG(newkey_template.size()), &newkey) != CKR_OK)
		return {};

	return d->attribute(d->session, newkey, CKA_VALUE);
}

QByteArray QPKCS11::deriveConcatKDF(const QByteArray &publicKey, const QString &digest, int keySize,
	const QByteArray &algorithmID, const QByteArray &partyUInfo, const QByteArray &partyVInfo) const
{
	return CryptoDoc::concatKDF(digest, quint32(keySize), derive(publicKey), algorithmID + partyUInfo + partyVInfo);
}

QByteArray QPKCS11::decrypt( const QByteArray &data ) const
{
	QByteArray result;
	std::vector<CK_OBJECT_HANDLE> key = d->findObject(d->session, CKO_PRIVATE_KEY, d->id);
	if(key.size() != 1)
		return result;

	CK_MECHANISM mech = { CKM_RSA_PKCS, nullptr, 0 };
	if(d->f->C_DecryptInit(d->session, &mech, key[0]) != CKR_OK)
		return result;

	CK_ULONG size = 0;
	if(d->f->C_Decrypt(d->session, CK_BYTE_PTR(data.constData()), CK_ULONG(data.size()), nullptr, &size) != CKR_OK)
		return result;

	result.resize(int(size));
	if(d->f->C_Decrypt(d->session, CK_BYTE_PTR(data.constData()), CK_ULONG(data.size()), CK_BYTE_PTR(result.data()), &size) != CKR_OK)
		result.clear();
	return result;
}

bool QPKCS11::isLoaded() const
{
	return d->f != nullptr;
}

bool QPKCS11::load( const QString &driver )
{
	if(d->lib.fileName() == driver && isLoaded())
		return true;
	qWarning() << "Loading:" << driver;
	unload();
	d->lib.setFileName( driver );
	CK_C_GetFunctionList l = CK_C_GetFunctionList(d->lib.resolve( "C_GetFunctionList" ));
	if( !l || l( &d->f ) != CKR_OK )
	{
		qWarning() << "Failed to resolve symbols";
		return false;
	}

	CK_C_INITIALIZE_ARGS init_args = { nullptr, nullptr, nullptr, nullptr, CKF_OS_LOCKING_OK, nullptr };
	CK_RV err = d->f->C_Initialize( &init_args );
	if( err != CKR_OK && err != CKR_CRYPTOKI_ALREADY_INITIALIZED )
	{
		qWarning() << "Failed to initalize";
		return false;
	}

	CK_INFO info;
	d->f->C_GetInfo( &info );
	qWarning()
		<< QStringLiteral("%1 (%2.%3)").arg(toQByteArray(info.manufacturerID).constData())
			.arg(info.cryptokiVersion.major).arg(info.cryptokiVersion.minor) << endl
		<< QStringLiteral("%1 (%2.%3)").arg(toQByteArray(info.libraryDescription).constData())
			.arg(info.libraryVersion.major).arg(info.libraryVersion.minor) << endl
		<< "Flags:" << info.flags;
	d->isFinDriver = toQByteArray(info.libraryDescription).contains("MPOLLUX");
	return true;
}

QPKCS11::PinStatus QPKCS11::lastError() const
{
	return d->lastError;
}

QPKCS11::PinStatus QPKCS11::login(const TokenData &t)
{
	logout();

	CK_SLOT_ID currentSlot = t.data(QStringLiteral("slot")).value<CK_SLOT_ID>();
	d->id = t.data(QStringLiteral("id")).toByteArray();

	CK_TOKEN_INFO token;
	if(d->f->C_GetTokenInfo(currentSlot, &token) != CKR_OK ||
		d->f->C_OpenSession(currentSlot, CKF_SERIAL_SESSION, nullptr, nullptr, &d->session) != CKR_OK)
		return UnknownError;

	std::vector<CK_OBJECT_HANDLE> list = d->findObject(d->session, CKO_CERTIFICATE, d->id);
	if(list.size() != 1 || QSslCertificate(d->attribute(d->session, list[0], CKA_VALUE), QSsl::Der) != t.cert())
		return UnknownError;

	// Hack: Workaround broken FIN pkcs11 drivers not providing CKF_LOGIN_REQUIRED info
	if(!d->isFinDriver && !(token.flags & CKF_LOGIN_REQUIRED))
		return PinOK;

	CK_RV err = CKR_OK;
	SslCertificate cert(t.cert());
	bool isSign = cert.keyUsage().keys().contains(SslCertificate::NonRepudiation);
	PinPopup::TokenFlags f;
	if(token.flags & CKF_USER_PIN_LOCKED) return PinLocked;
	if(token.flags & CKF_USER_PIN_COUNT_LOW) f = PinPopup::PinCountLow;
	if(token.flags & CKF_USER_PIN_FINAL_TRY) f = PinPopup::PinFinalTry;
	if(token.flags & CKF_PROTECTED_AUTHENTICATION_PATH)
	{
		PinPopup p(isSign ? PinPopup::Pin2PinpadType : PinPopup::Pin1PinpadType, cert, f, rootWindow());
		connect(d, &Private::started, &p, &PinPopup::startTimer);
		connect(d, &Private::finished, &p, &PinPopup::accept);
		d->start();
		p.exec();
		err = d->result;
	}
	else
	{
		PinPopup p(isSign ? PinPopup::Pin2Type : PinPopup::Pin1Type, cert, f, rootWindow());
		p.setPinLen(token.ulMinPinLen, token.ulMaxPinLen < 12 ? 12 : token.ulMaxPinLen);
		if( !p.exec() )
			return PinCanceled;
		QByteArray pin = p.text().toUtf8();
		err = d->f->C_Login(d->session, CKU_USER, CK_UTF8CHAR_PTR(pin.constData()), CK_ULONG(pin.size()));
	}

	d->f->C_GetTokenInfo(currentSlot, &token);

	switch( err )
	{
	case CKR_OK:
	case CKR_USER_ALREADY_LOGGED_IN: return PinOK;
	case CKR_CANCEL:
	case CKR_FUNCTION_CANCELED: return PinCanceled;
	case CKR_PIN_INCORRECT: return (token.flags & CKF_USER_PIN_LOCKED) ? PinLocked : PinIncorrect;
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

QList<TokenData> QPKCS11::tokens() const
{
	QList<TokenData> list;
	for( CK_SLOT_ID slot: d->slotIds( true ) )
	{
		CK_SLOT_INFO slotInfo;
		CK_TOKEN_INFO token;
		CK_SESSION_HANDLE session = 0;
		if(d->f->C_GetSlotInfo(slot, &slotInfo) != CKR_OK ||
			d->f->C_GetTokenInfo(slot, &token) != CKR_OK ||
			d->f->C_OpenSession(slot, CKF_SERIAL_SESSION, nullptr, nullptr, &session) != CKR_OK)
			continue;
		for( CK_OBJECT_HANDLE obj: d->findObject( session, CKO_CERTIFICATE ) )
		{
			SslCertificate cert(d->attribute(session, obj, CKA_VALUE), QSsl::Der);
			if(cert.isCA())
				continue;
			QByteArray id = d->attribute(session, obj, CKA_ID);
			// Hack: Workaround broken FIN pkcs11 drivers showing non-repu certificates in auth slot
			if(d->isFinDriver && d->findObject(session, CKO_PUBLIC_KEY, id).empty())
				continue;
			TokenData t;
			t.setCard(cert.type() & SslCertificate::EstEidType || cert.type() & SslCertificate::DigiIDType ?
				toQByteArray(token.serialNumber).trimmed() : cert.subjectInfo(QSslCertificate::CommonName) + "-" + cert.serialNumber());
			t.setCert(cert);
			t.setReader(QByteArray::fromRawData((const char*)slotInfo.slotDescription, sizeof(slotInfo.slotDescription)).trimmed());
			t.setData(QStringLiteral("slot"), QVariant::fromValue(slot));
			t.setData(QStringLiteral("id"), id);
			list << t;
		}
		d->f->C_CloseSession( session );
	}
	return list;
}

bool QPKCS11::reload()
{
	static QMultiHash<QString,QByteArray> drivers {
#ifdef Q_OS_MAC
		{ qApp->applicationDirPath() + "/opensc-pkcs11.so", QByteArray() },
		{ "/Library/latvia-eid/lib/eidlv-pkcs11.bundle/Contents/MacOS/eidlv-pkcs11", "3BDD18008131FE45904C41545649412D65494490008C" },
		{ "/Library/latvia-eid/lib/eidlv-pkcs11.bundle/Contents/MacOS/eidlv-pkcs11", "3BDB960080B1FE451F830012428F536549440F900020" },
		{ "/Library/Security/tokend/CCSuite.tokend/Contents/Frameworks/libccpkip11.dylib", "3BF81300008131FE45536D617274417070F8" },
		{ "/Library/Security/tokend/CCSuite.tokend/Contents/Frameworks/libccpkip11.dylib", "3B7D94000080318065B08311C0A983009000" },
		{ "/Library/mPolluxDigiSign/libcryptoki.dylib", "3B7F9600008031B865B0850300EF1200F6829000" },
		{ "/Library/mPolluxDigiSign/libcryptoki.dylib", "3B7B940000806212515646696E454944" },
		{ "/Library/Frameworks/eToken.framework/Versions/Current/libeToken.dylib", "3BD5180081313A7D8073C8211030" },
		{ "/Library/Frameworks/eToken.framework/Versions/Current/libeToken.dylib", "3BD518008131FE7D8073C82110F4" },
		{ "/Library/Frameworks/eToken.framework/Versions/Current/libIDPrimePKCS11.dylib", "3BFF9600008131804380318065B0850300EF120FFE82900066" },
#elif defined(Q_OS_WIN)
		{ "opensc-pkcs11.dll", QByteArray() },
		{ qApp->applicationDirPath() + "/../CryptoTech/CryptoCard/CCPkiP11.dll", "3BF81300008131FE45536D617274417070F8" },
		{ qApp->applicationDirPath() + "/../CryptoTech/CryptoCard/CCPkiP11.dll", "3B7D94000080318065B08311C0A983009000" },
#else
		{ "opensc-pkcs11.so", QByteArray() },
		{ "/opt/latvia-eid/lib/eidlv-pkcs11.so", "3BDD18008131FE45904C41545649412D65494490008C" },
		{ "/opt/latvia-eid/lib/eidlv-pkcs11.so", "3BDB960080B1FE451F830012428F536549440F900020" },
#if Q_PROCESSOR_WORDSIZE == 8
		{ "/usr/lib64/pwpw-card-pkcs11.so", "3BF81300008131FE45536D617274417070F8" },
		{ "/usr/lib64/pwpw-card-pkcs11.so", "3B7D94000080318065B08311C0A983009000" },
		{ "/usr/lib64/libcryptoki.so", "3B7F9600008031B865B0850300EF1200F6829000" },
		{ "/usr/lib64/libcryptoki.so", "3B7B940000806212515646696E454944" },
#else
		{ "pwpw-card-pkcs11.so", "3BF81300008131FE45536D617274417070F8" },
		{ "pwpw-card-pkcs11.so", "3B7D94000080318065B08311C0A983009000" },
		{ "libcryptoki.so", "3B7F9600008031B865B0850300EF1200F6829000" },
		{ "libcryptoki.so", "3B7B940000806212515646696E454944" },
#endif
		{ "/usr/lib/libeTPkcs11.so", "3BD5180081313A7D8073C8211030" },
		{ "/usr/lib/libeTPkcs11.so", "3BD518008131FE7D8073C82110F4" },
		{ "/usr/lib/libIDPrimePKCS11.so", "3BFF9600008131804380318065B0850300EF120FFE82900066" },
#endif
	};
	for(const QString &reader: QPCSC::instance().readers())
	{
		QPCSCReader r(reader, &QPCSC::instance());
		if(!r.isPresent())
			continue;
		QString driver = drivers.key(r.atr());
		if(!driver.isEmpty() && load(driver))
			return true;
	}
	return load(drivers.key(QByteArray()));
}

QByteArray QPKCS11::sign( int type, const QByteArray &digest ) const
{
	QByteArray sig;
	std::vector<CK_OBJECT_HANDLE> key = d->findObject(d->session, CKO_PRIVATE_KEY, d->id);
	if(key.size() != 1)
		return sig;

	CK_KEY_TYPE keyType = CKK_RSA;
	CK_ATTRIBUTE attribute = { CKA_KEY_TYPE, &keyType, sizeof(keyType) };
	d->f->C_GetAttributeValue(d->session, key[0], &attribute, 1);

	CK_MECHANISM mech = { keyType == CKK_ECDSA ? CKM_ECDSA : CKM_RSA_PKCS, nullptr, 0 };
	if(d->f->C_SignInit(d->session, &mech, key[0]) != CKR_OK)
		return sig;

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
	if(d->f->C_Sign(d->session, CK_BYTE_PTR(data.constData()), CK_ULONG(data.size()), nullptr, &size) != CKR_OK)
		return sig;

	sig.resize(int(size));
	if(d->f->C_Sign(d->session, CK_BYTE_PTR(data.constData()), CK_ULONG(data.size()), CK_BYTE_PTR(sig.data()), &size) != CKR_OK)
		sig.clear();
	return sig;
}

void QPKCS11::unload()
{
	logout();
	if(d->f)
		d->f->C_Finalize(nullptr);
	d->f = nullptr;
	d->lib.unload();
}
