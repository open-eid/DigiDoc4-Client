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

#include "QCryptoBackend.h"

#include "TokenData.h"
#ifdef Q_OS_WIN
#include "QCNG.h"
#endif
#include "QPKCS11.h"

#include <QtNetwork/QSslKey>
#include <QtNetwork/QSslCertificate>

// TODO: Port everything to the new OpenSSL API
#define OPENSSL_SUPPRESS_DEPRECATED

#include <openssl/ecdsa.h>
#include <openssl/obj_mac.h>
#include <openssl/rsa.h>

std::expected<QCryptoBackend *,QCryptoBackend::Status>
QCryptoBackend::getBackend(const TokenData& token) {
#ifdef Q_OS_WIN
	auto backend = std::make_unique<QCNG>();
#else
	auto backend = std::make_unique<QPKCS11>();
#endif
	backend.get()->cert = token.cert();
	Status status;
	do {
		status = backend->login(token);
	} while (status == PinIncorrect);
	if (status != PinOK) return std::unexpected(status);
	return backend.release();
}

QList<TokenData>
QCryptoBackend::getTokens()
{
#ifdef Q_OS_WIN
	return QCNG::tokens();
#else
	return QPKCS11::tokens();
#endif
}

QString QCryptoBackend::errorString(Status error)
{
	switch( error )
	{
	case PinOK: return QString();
	case PinCanceled: return QObject::tr("PIN Canceled");
	case PinLocked: return QObject::tr("PIN locked");
	case PinIncorrect: return QObject::tr("PIN Incorrect");
	case GeneralError: return QObject::tr("PKCS11 general error");
	case DeviceError: return QObject::tr("PKCS11 device error");
	default: return QObject::tr("Unknown error");
	}
}

static int rsa_sign(int type, const unsigned char *m, unsigned int m_len, unsigned char *sigret, unsigned int *siglen, const RSA *rsa)
{
	auto *backend = (QCryptoBackend*) RSA_get_ex_data(rsa, 0);
	QCryptographicHash::Algorithm algo = QCryptographicHash::Sha256;
	switch(type)
	{
	case NID_sha224: algo = QCryptographicHash::Sha224; break;
	case NID_sha256: algo = QCryptographicHash::Sha256; break;
	case NID_sha384: algo = QCryptographicHash::Sha384; break;
	case NID_sha512: algo = QCryptographicHash::Sha512; break;
	}
	QByteArray result = backend->sign(algo, QByteArray::fromRawData((const char*)m, int(m_len)));
	if(result.isEmpty())
		return 0;
	*siglen = (unsigned int)result.size();
	memcpy(sigret, result.constData(), size_t(result.size()));
	return 1;
}

static ECDSA_SIG*
ecdsa_do_sign(const unsigned char *dgst, int dgst_len, const BIGNUM * /*inv*/, const BIGNUM * /*rp*/, EC_KEY *eckey)
{
	auto *backend = (QCryptoBackend*)EC_KEY_get_ex_data(eckey, 0);
	QByteArray result = backend->sign(QCryptographicHash::Sha512, QByteArray::fromRawData((const char*)dgst, dgst_len));
	if(result.isEmpty())
		return nullptr;
	QByteArray r = result.left(result.size()/2);
	QByteArray s = result.right(result.size()/2);
	ECDSA_SIG *sig = ECDSA_SIG_new();
	ECDSA_SIG_set0(sig,
		BN_bin2bn((const unsigned char*)r.data(), int(r.size()), nullptr),
		BN_bin2bn((const unsigned char*)s.data(), int(s.size()), nullptr));
	return sig;
}

static RSA_METHOD *get_rsa_method(bool release = false)
{
	static RSA_METHOD *method = nullptr;
	if (!method && !release) {
		method = RSA_meth_dup(RSA_get_default_method());
		RSA_meth_set1_name(method, "QSmartCard");
		RSA_meth_set_sign(method, rsa_sign);
	} else if (method && release) {
		RSA_meth_free(method);
		method = nullptr;
	}
	return method;
}

static EC_KEY_METHOD *get_ec_method(bool release = false)
{
	static EC_KEY_METHOD *method = nullptr;
	if(!method && !release) {
		method = EC_KEY_METHOD_new(EC_KEY_get_default_method());
		using EC_KEY_sign = int (*)(int type, const unsigned char *dgst, int dlen, unsigned char *sig,
			unsigned int *siglen, const BIGNUM *kinv, const BIGNUM *r, EC_KEY *eckey);
		using EC_KEY_sign_setup = int (*)(EC_KEY *eckey, BN_CTX *ctx_in, BIGNUM **kinvp, BIGNUM **rp);
		EC_KEY_sign sign = nullptr;
		EC_KEY_sign_setup sign_setup = nullptr;
		EC_KEY_METHOD_get_sign(method, &sign, &sign_setup, nullptr);
		EC_KEY_METHOD_set_sign(method, sign, sign_setup, ecdsa_do_sign);
	} else if (method && release) {
		EC_KEY_METHOD_free(method);
		method = nullptr;
	}
	return method;
}

QSslKey
QCryptoBackend::getKey()
{
	QSslKey key = cert.publicKey();
	if(!key.handle()) {
		status = GeneralError;
		return {};
	}
	if(key.algorithm() == QSsl::Ec)
	{
		auto *ec = (EC_KEY*)key.handle();
		EC_KEY_set_method(ec, get_ec_method());
		EC_KEY_set_ex_data(ec, 0, this);
	}
	else
	{
		RSA *rsa = (RSA*)key.handle();
		RSA_set_method(rsa, get_rsa_method());
		RSA_set_ex_data(rsa, 0, this);
	}
	return key;
}

void
QCryptoBackend::shutDown()
{
	get_rsa_method(true);
	get_ec_method(true);
}

