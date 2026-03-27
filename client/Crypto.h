// SPDX-FileCopyrightText: Estonian Information System Authority
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include <QtCore/QCryptographicHash>
#include <QtCore/QLoggingCategory>

#include <memory>

using EVP_CIPHER = struct evp_cipher_st;
using EVP_CIPHER_CTX = struct evp_cipher_ctx_st;
using EVP_PKEY = struct evp_pkey_st;
using puchar = uchar *;
using pcuchar = const uchar *;
class QSslKey;

#define SCOPE(TYPE, DATA) std::unique_ptr<TYPE,decltype(&TYPE##_free)>(static_cast<TYPE*>(DATA), TYPE##_free)

Q_DECLARE_LOGGING_CATEGORY(CRYPTO)

class Crypto
{
public:
	struct Cipher {
		std::unique_ptr<EVP_CIPHER_CTX,void (*)(EVP_CIPHER_CTX*)> ctx;
		Cipher(const EVP_CIPHER *cipher, const QByteArray &key, const QByteArray &iv, bool encrypt = true);
		bool updateAAD(const QByteArray &data) const;
		QByteArray update(const QByteArray &data) const;
		bool update(char *data, int size) const;
		bool result() const;
		QByteArray tag() const;
		static constexpr int tagLen() { return 16; }
		bool setTag(const QByteArray &data) const;
	};

	static QByteArray aes_wrap(const QByteArray &key, const QByteArray &data, bool encrypt);
	static QByteArray cipher(const EVP_CIPHER *cipher, const QByteArray &key, QByteArray &data, bool encrypt);
	static QByteArray curve_oid(EVP_PKEY *key);
	static QByteArray concatKDF(QCryptographicHash::Algorithm digestMethod, const QByteArray &z, const QByteArray &otherInfo);
	static QByteArray derive(EVP_PKEY *priv, EVP_PKEY *pub);
	static QByteArray encrypt(EVP_PKEY *pub, int padding, const QByteArray &data);
	static QByteArray expand(const QByteArray &key, const QByteArray &info, int len = 32);
	static QByteArray extract(const QByteArray &key, const QByteArray &salt, int len = 32);
	static std::unique_ptr<EVP_PKEY, void (*)(EVP_PKEY *)> fromPUBKeyDer(const QByteArray &key);
	static std::unique_ptr<EVP_PKEY, void (*)(EVP_PKEY *)> fromECPublicKeyDer(const QByteArray &key, int curveName);
	static std::unique_ptr<EVP_PKEY, void (*)(EVP_PKEY *)> fromRSAPublicKeyDer(const QByteArray &key);
	static std::unique_ptr<EVP_PKEY, void (*)(EVP_PKEY *)> genECKey(EVP_PKEY *params);
	static QByteArray genKey(const EVP_CIPHER *cipher);
	static QByteArray hkdf(const QByteArray &key, const QByteArray &salt, const QByteArray &info, int len = 32, int mode = 0);
	static QByteArray sign_hmac(const QByteArray &key, const QByteArray &data);
	static QByteArray toPublicKeyDer(EVP_PKEY *key);
	static QByteArray toPublicKeyDer(const QSslKey &key);
	static QByteArray random(int len = 32);
	static QByteArray xor_data(const QByteArray &a, const QByteArray &b);

private:
	static bool isError(int err);
};
