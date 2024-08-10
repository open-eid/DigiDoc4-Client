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

#include "Application.h"
#include "Crypto.h"
#include "SslCertificate.h"
#include "TokenData.h"
#include "dialogs/PinPopup.h"

#include <common/QPCSC.h>

#include <QtCore/QDebug>

#include <array>

template<class Container>
static QString toQString(const Container &c)
{
	return QString::fromLatin1((const char*)std::data(c), std::size(c));
}

QByteArray QPKCS11::Private::attribute(CK_SESSION_HANDLE session, CK_OBJECT_HANDLE obj, CK_ATTRIBUTE_TYPE type) const
{
	if(!f)
		return {};
	CK_ATTRIBUTE attr { type, nullptr, 0 };
	if( f->C_GetAttributeValue( session, obj, &attr, 1 ) != CKR_OK )
		return {};
	QByteArray data(int(attr.ulValueLen), 0);
	attr.pValue = data.data();
	if( f->C_GetAttributeValue( session, obj, &attr, 1 ) != CKR_OK )
		return {};
	return data;
}

std::vector<CK_OBJECT_HANDLE> QPKCS11::Private::findObject(CK_SESSION_HANDLE session, CK_OBJECT_CLASS cls, const QByteArray &id) const
{
	if(!f)
		return {};
	CK_BBOOL _true = CK_TRUE;
	std::vector<CK_ATTRIBUTE> attr{
		{ CKA_CLASS, &cls, sizeof(cls) },
		{ CKA_TOKEN, &_true, sizeof(_true) }
	};
	if(!id.isEmpty())
		attr.push_back({ CKA_ID, CK_VOID_PTR(id.data()), CK_ULONG(id.size()) });
	if(f->C_FindObjectsInit(session, attr.data(), CK_ULONG(attr.size())) != CKR_OK)
		return {};

	CK_ULONG count = 32;
	std::vector<CK_OBJECT_HANDLE> result(size_t(count), CK_INVALID_HANDLE);
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

QByteArray QPKCS11::decrypt(const QByteArray &data, bool oaep) const
{
	std::vector<CK_OBJECT_HANDLE> key = d->findObject(d->session, CKO_PRIVATE_KEY, d->id);
	if(key.size() != 1)
		return {};

	CK_RSA_PKCS_OAEP_PARAMS params { CKM_SHA256, CKG_MGF1_SHA256, 0, nullptr, 0 };
	auto mech = oaep ?
		CK_MECHANISM{ CKM_RSA_PKCS_OAEP, &params, sizeof(params) } :
		CK_MECHANISM{ CKM_RSA_PKCS, nullptr, 0 };
	if(d->f->C_DecryptInit(d->session, &mech, key.front()) != CKR_OK)
		return {};

	CK_ULONG size = 0;
	if(d->f->C_Decrypt(d->session, CK_BYTE_PTR(data.constData()), CK_ULONG(data.size()), nullptr, &size) != CKR_OK)
		return {};

	QByteArray result(int(size), 0);
	if(d->f->C_Decrypt(d->session, CK_BYTE_PTR(data.constData()), CK_ULONG(data.size()), CK_BYTE_PTR(result.data()), &size) != CKR_OK)
		return {};
	return result;
}

QByteArray QPKCS11::derive(const QByteArray &publicKey) const
{
	std::vector<CK_OBJECT_HANDLE> key = d->findObject(d->session, CKO_PRIVATE_KEY, d->id);
	if(key.size() != 1)
		return {};

	CK_ECDH1_DERIVE_PARAMS ecdh_parms { CKD_NULL, 0, nullptr, CK_ULONG(publicKey.size()), CK_BYTE_PTR(publicKey.data()) };
	CK_MECHANISM mech { CKM_ECDH1_DERIVE, &ecdh_parms, sizeof(CK_ECDH1_DERIVE_PARAMS) };
	CK_BBOOL _false = CK_FALSE;
	CK_OBJECT_CLASS newkey_class = CKO_SECRET_KEY;
	CK_KEY_TYPE newkey_type = CKK_GENERIC_SECRET;
	std::array newkey_template{
		CK_ATTRIBUTE{CKA_TOKEN, &_false, sizeof(_false)},
		CK_ATTRIBUTE{CKA_CLASS, &newkey_class, sizeof(newkey_class)},
		CK_ATTRIBUTE{CKA_KEY_TYPE, &newkey_type, sizeof(newkey_type)},
	};
	CK_OBJECT_HANDLE newkey = CK_INVALID_HANDLE;
	if(d->f->C_DeriveKey(d->session, &mech, key.front(), newkey_template.data(), CK_ULONG(newkey_template.size()), &newkey) != CKR_OK)
		return {};

	return d->attribute(d->session, newkey, CKA_VALUE);
}

QByteArray QPKCS11::deriveConcatKDF(const QByteArray &publicKey, QCryptographicHash::Algorithm digest,
	int keySize, const QByteArray &algorithmID, const QByteArray &partyUInfo, const QByteArray &partyVInfo) const
{
	return Crypto::concatKDF(digest, quint32(keySize), derive(publicKey), algorithmID + partyUInfo + partyVInfo);
}

QByteArray QPKCS11::deriveHMACExtract(const QByteArray &publicKey, const QByteArray &salt, int keySize) const
{
	return Crypto::extract(derive(publicKey), salt, keySize);
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
	if(auto l = CK_C_GetFunctionList(d->lib.resolve("C_GetFunctionList"));
		!l || l(&d->f) != CKR_OK)
	{
		qWarning() << "Failed to resolve symbols";
		return false;
	}

	CK_C_INITIALIZE_ARGS init_args { nullptr, nullptr, nullptr, nullptr, CKF_OS_LOCKING_OK, nullptr };
	CK_RV err = d->f->C_Initialize( &init_args );
	if( err != CKR_OK && err != CKR_CRYPTOKI_ALREADY_INITIALIZED )
	{
		qWarning() << "Failed to initalize";
		return false;
	}

	CK_INFO info{};
	d->f->C_GetInfo( &info );
	qWarning()
		<< QStringLiteral("%1 (%2.%3)").arg(toQString(info.manufacturerID))
			.arg(info.cryptokiVersion.major).arg(info.cryptokiVersion.minor) << '\n'
		<< QStringLiteral("%1 (%2.%3)").arg(toQString(info.libraryDescription))
			.arg(info.libraryVersion.major).arg(info.libraryVersion.minor) << '\n'
		<< "Flags:" << info.flags;
	d->isFinDriver = toQString(info.libraryDescription).contains(QLatin1String("MPOLLUX"), Qt::CaseInsensitive);
	return true;
}

QPKCS11::PinStatus QPKCS11::login(const TokenData &t)
{
	logout();

	auto currentSlot = t.data(QStringLiteral("slot")).value<CK_SLOT_ID>();
	d->id = t.data(QStringLiteral("id")).toByteArray();
	d->isPSS = t.data(QStringLiteral("PSS")).toBool();

	CK_TOKEN_INFO token;
	if(d->f->C_GetTokenInfo(currentSlot, &token) != CKR_OK ||
		d->f->C_OpenSession(currentSlot, CKF_SERIAL_SESSION, nullptr, nullptr, &d->session) != CKR_OK)
		return UnknownError;

	std::vector<CK_OBJECT_HANDLE> list = d->findObject(d->session, CKO_CERTIFICATE, d->id);
	if(list.size() != 1 || QSslCertificate(d->attribute(d->session, list.front(), CKA_VALUE), QSsl::Der) != t.cert())
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
		PinPopup p(isSign ? PinPopup::Pin2PinpadType : PinPopup::Pin1PinpadType, cert, f, Application::mainWindow());
		connect(d, &Private::started, &p, &PinPopup::startTimer);
		connect(d, &Private::finished, &p, &PinPopup::accept);
		d->start();
		p.exec();
		err = d->result;
	}
	else
	{
		PinPopup p(isSign ? PinPopup::Pin2Type : PinPopup::Pin1Type, cert, f, Application::mainWindow());
		p.setPinLen(token.ulMinPinLen, token.ulMaxPinLen < 12 ? 12 : token.ulMaxPinLen);
		if( !p.exec() )
			return PinCanceled;
		QByteArray pin = p.pin().toUtf8();
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
	if(!d->f)
		return list;
	size_t size = 0;
	if(d->f->C_GetSlotList(CK_TRUE, nullptr, CK_ULONG_PTR(&size)) != CKR_OK)
		return list;
	std::vector<CK_SLOT_ID> slotIDs(size);
	if(d->f->C_GetSlotList(CK_TRUE, slotIDs.data(), CK_ULONG_PTR(&size)) != CKR_OK)
		return list;
	slotIDs.resize(size);
	for(CK_SLOT_ID slot: slotIDs)
	{
		CK_SLOT_INFO slotInfo{};
		CK_TOKEN_INFO token{};
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
			auto key = d->findObject(session, CKO_PUBLIC_KEY, id);
			if(key.size() != 1) // Workaround broken FIN pkcs11 drivers showing non-repu certificates in auth slot
				continue;
			TokenData t;
			t.setCard(cert.type() & SslCertificate::EstEidType || cert.type() & SslCertificate::DigiIDType ?
				toQString(token.serialNumber).trimmed() : cert.subjectInfo(QSslCertificate::CommonName) + "-" + cert.serialNumber());
			t.setCert(cert);
			t.setReader(toQString(slotInfo.slotDescription).trimmed());
			t.setData(QStringLiteral("slot"), QVariant::fromValue(slot));
			t.setData(QStringLiteral("id"), id);

			CK_KEY_TYPE keyType = CKK_RSA;
			CK_ATTRIBUTE attribute { CKA_KEY_TYPE, &keyType, sizeof(keyType) };
			d->f->C_GetAttributeValue(session, key.front(), &attribute, 1);

			if(keyType == CKK_RSA)
			{
				CK_ULONG count = 0;
				d->f->C_GetMechanismList(slot, nullptr, &count);
				QVector<CK_MECHANISM_TYPE> mech(count);
				d->f->C_GetMechanismList(slot, mech.data(), &count);
				t.setData(QStringLiteral("PSS"), mech.contains(CKM_RSA_PKCS_PSS));
			}
			list.append(std::move(t));
		}
		d->f->C_CloseSession( session );
	}
	return list;
}

bool QPKCS11::reload()
{
	static QMultiHash<QString,QByteArray> drivers {
#ifdef Q_OS_MAC
		{ QApplication::applicationDirPath() + "/opensc-pkcs11.so", {} },
		{ "/Library/latvia-eid/lib/eidlv-pkcs11.bundle/Contents/MacOS/eidlv-pkcs11", "3BDD18008131FE45904C41545649412D65494490008C" }, // LV-G1
		{ "/Library/latvia-eid/lib/eidlv-pkcs11.bundle/Contents/MacOS/eidlv-pkcs11", "3BDB960080B1FE451F830012428F536549440F900020" }, // LV-G2
		{ "/Library/mCard/lib/mcard-pkcs11.so", "3B9D188131FC358031C0694D54434F5373020505D3" }, // LT-G3
		{ "/Library/mCard/lib/mcard-pkcs11.so", "3B9D188131FC358031C0694D54434F5373020604D1" }, // LT-G3.1
		{ "/Library/mPolluxDigiSign/libcryptoki.dylib", "3B7F9600008031B865B0850300EF1200F6829000" }, // FI-G3
		{ "/Library/mPolluxDigiSign/libcryptoki.dylib", "3B7F9600008031B865B08504021B1200F6829000" }, // FI-G3.1
		{ "/Library/mPolluxDigiSign/libcryptoki.dylib", "3B7F9600008031B865B085050011122460829000" }, // FI-G4
		{ "/Library/Frameworks/eToken.framework/Versions/Current/libeToken.dylib", "3BD5180081313A7D8073C8211030" },
		{ "/Library/Frameworks/eToken.framework/Versions/Current/libeToken.dylib", "3BD518008131FE7D8073C82110F4" },
		{ "/Library/Frameworks/eToken.framework/Versions/Current/libeToken.dylib", "3BFF9600008131FE4380318065B0846566FB12017882900085" },
		{ "/Library/Frameworks/eToken.framework/Versions/Current/libIDPrimePKCS11.dylib", "3BFF9600008131804380318065B0850300EF120FFE82900066" },
		{ "/Library/Frameworks/eToken.framework/Versions/Current/libIDPrimePKCS11.dylib", "3BFF9600008131FE4380318065B0855956FB120FFE82900000" },
#elif defined(Q_OS_WIN)
		{ "opensc-pkcs11.dll", {} },
#else
		{ "opensc-pkcs11.so", {} },
#if defined(Q_OS_LINUX)
		{ "/opt/latvia-eid/lib/eidlv-pkcs11.so", "3BDD18008131FE45904C41545649412D65494490008C" }, // LV-G1
		{ "/opt/latvia-eid/lib/eidlv-pkcs11.so", "3BDB960080B1FE451F830012428F536549440F900020" }, // LV-G2
		{ "mcard-pkcs11.so", "3B9D188131FC358031C0694D54434F5373020505D3" }, // LT-G3
		{ "mcard-pkcs11.so", "3B9D188131FC358031C0694D54434F5373020604D1" }, // LT-G3.1
#if Q_PROCESSOR_WORDSIZE == 8
		{ "/usr/lib64/libcryptoki.so", "3B7F9600008031B865B0850300EF1200F6829000" }, // FI-G3
		{ "/usr/lib64/libcryptoki.so", "3B7F9600008031B865B08504021B1200F6829000" }, // FI-G3.1
		{ "/usr/lib64/libcryptoki.so", "3B7F9600008031B865B085050011122460829000" }, // FI-G4
#else
		{ "libcryptoki.so", "3B7F9600008031B865B0850300EF1200F6829000" }, // FI-G3
		{ "libcryptoki.so", "3B7F9600008031B865B08504021B1200F6829000" }, // FI-G3.1
		{ "libcryptoki.so", "3B7F9600008031B865B085050011122460829000" }, // FI-G4
#endif
		{ "/usr/lib/libeTPkcs11.so", "3BD5180081313A7D8073C8211030" },
		{ "/usr/lib/libeTPkcs11.so", "3BD518008131FE7D8073C82110F4" },
		{ "/usr/lib/libeTPkcs11.so", "3BFF9600008131FE4380318065B0846566FB12017882900085" },
		{ "/usr/lib/libIDPrimePKCS11.so", "3BFF9600008131804380318065B0850300EF120FFE82900066" },
		{ "/usr/lib/libIDPrimePKCS11.so", "3BFF9600008131FE4380318065B0855956FB120FFE82900000" },
#endif
#endif
	};
	for(const QString &reader: QPCSC::instance().readers())
	{
		QPCSCReader r(reader, &QPCSC::instance());
		if(!r.isPresent())
			continue;
		qDebug() << r.atr();
		QString driver = drivers.key(r.atr());
		if(!driver.isEmpty() && load(driver))
			return true;
	}
	return load(drivers.key({}));
}

QByteArray QPKCS11::sign(QCryptographicHash::Algorithm type, const QByteArray &digest) const
{
	std::vector<CK_OBJECT_HANDLE> key = d->findObject(d->session, CKO_PRIVATE_KEY, d->id);
	if(key.size() != 1)
		return {};

	CK_KEY_TYPE keyType = CKK_RSA;
	CK_ATTRIBUTE attribute { CKA_KEY_TYPE, &keyType, sizeof(keyType) };
	d->f->C_GetAttributeValue(d->session, key.front(), &attribute, 1);

	CK_RSA_PKCS_PSS_PARAMS pssParams { CKM_SHA256, CKG_MGF1_SHA256, 32 };
	CK_MECHANISM mech { keyType == CKK_ECDSA ? CKM_ECDSA : CKM_RSA_PKCS, nullptr, 0 };
	QByteArray data;
	if(keyType == CKK_RSA)
	{
		switch(type)
		{
		case QCryptographicHash::Sha224:
			data = QByteArray::fromHex("302d300d06096086480165030402040500041c");
			pssParams = { CKM_SHA224, CKG_MGF1_SHA224, 24 };
			break;
		case QCryptographicHash::Sha256:
			data = QByteArray::fromHex("3031300d060960864801650304020105000420");
			pssParams = { CKM_SHA256, CKG_MGF1_SHA256, 32 };
			break;
		case QCryptographicHash::Sha384:
			data = QByteArray::fromHex("3041300d060960864801650304020205000430");
			pssParams = { CKM_SHA384, CKG_MGF1_SHA384, 48 };
			break;
		case QCryptographicHash::Sha512:
			data = QByteArray::fromHex("3051300d060960864801650304020305000440");
			pssParams = { CKM_SHA512, CKG_MGF1_SHA512, 64 };
			break;
		default: break;
		}
		if(d->isPSS)
		{
			mech = { CKM_RSA_PKCS_PSS, &pssParams, sizeof(CK_RSA_PKCS_PSS_PARAMS) };
			data.clear();
		}
	}
	data.append(digest);

	if(d->f->C_SignInit(d->session, &mech, key.front()) != CKR_OK)
		return {};
	CK_ULONG size = 0;
	if(d->f->C_Sign(d->session, CK_BYTE_PTR(data.constData()), CK_ULONG(data.size()), nullptr, &size) != CKR_OK)
		return {};
	QByteArray sig(int(size), 0);
	if(d->f->C_Sign(d->session, CK_BYTE_PTR(data.constData()), CK_ULONG(data.size()), CK_BYTE_PTR(sig.data()), &size) != CKR_OK)
		return {};
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
