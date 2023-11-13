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

#include "Crypto.h"

#include <QtCore/QDebug>
#include <QtCore/QtEndian>
#include <QtNetwork/QSslKey>

#include <openssl/aes.h>
#include <openssl/ec.h>
#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/kdf.h>
#include <openssl/rand.h>
#include <openssl/rsa.h>
#include <openssl/x509.h>

#include <array>
#include <cmath>

Q_LOGGING_CATEGORY(CRYPTO,"CRYPTO")

Crypto::Cipher::Cipher(const EVP_CIPHER *cipher, const QByteArray &key, const QByteArray &iv, bool encrypt)
	: ctx(SCOPE(EVP_CIPHER_CTX, EVP_CIPHER_CTX_new()))
{
	EVP_CIPHER_CTX_set_flags(ctx.get(), EVP_CIPHER_CTX_FLAG_WRAP_ALLOW);
	Q_UNUSED(isError(EVP_CipherInit_ex(ctx.get(), cipher, nullptr, pcuchar(key.data()), iv.isEmpty() ? nullptr : pcuchar(iv.data()), int(encrypt))));
}

bool Crypto::Cipher::updateAAD(const QByteArray &data) const
{
	int len = 0;
	return !isError(EVP_CipherUpdate(ctx.get(), nullptr, &len, pcuchar(data.data()), int(data.size())));
}

QByteArray Crypto::Cipher::update(const QByteArray &data) const
{
	QByteArray result(data.size() + EVP_CIPHER_CTX_block_size(ctx.get()), 0);
	int len = int(result.size());
	if(isError(EVP_CipherUpdate(ctx.get(), puchar(result.data()), &len, pcuchar(data.data()), int(data.size()))))
		return {};
	result.resize(len);
	return result;
}

bool Crypto::Cipher::update(char *data, int size) const
{
	int len = 0;
	return !isError(EVP_CipherUpdate(ctx.get(), puchar(data), &len, pcuchar(data), size));
}

bool Crypto::Cipher::result() const
{
	QByteArray result(EVP_CIPHER_CTX_block_size(ctx.get()), 0);
	int len = int(result.size());
	if(isError(EVP_CipherFinal(ctx.get(), puchar(result.data()), &len)))
		return false;
	if(result.size() != len)
		result.resize(len);
	return true;
}

QByteArray Crypto::Cipher::tag() const
{
	if(QByteArray result(tagLen(), 0);
		!isError(EVP_CIPHER_CTX_ctrl(ctx.get(), EVP_CTRL_AEAD_GET_TAG, int(result.size()), result.data())))
		return result;
	return {};
}

bool Crypto::Cipher::setTag(const QByteArray &data) const
{
	return !isError(EVP_CIPHER_CTX_ctrl(ctx.get(), EVP_CTRL_AEAD_SET_TAG, int(data.size()), const_cast<char*>(data.data())));
}

QByteArray Crypto::aes_wrap(const QByteArray &key, const QByteArray &data, bool encrypt)
{
	Cipher c(key.size() == 32 ? EVP_aes_256_wrap() : EVP_aes_128_wrap(), key, {}, encrypt);
	if(QByteArray result = c.update(data); c.result())
		return result;
	return {};
}

QByteArray Crypto::cipher(const EVP_CIPHER *cipher, const QByteArray &key, QByteArray &data, bool encrypt)
{
	QByteArray iv(EVP_CIPHER_iv_length(cipher), 0), tag;
	if(!encrypt)
	{
		iv = data.left(iv.length());
		data.remove(0, iv.length());
		if(EVP_CIPHER_mode(cipher) == EVP_CIPH_GCM_MODE)
			tag = data.right(16);
		data.resize(data.size() - tag.size());
	}

	auto ctx = SCOPE(EVP_CIPHER_CTX, EVP_CIPHER_CTX_new());
	if(isError(EVP_CipherInit(ctx.get(), cipher, pcuchar(key.constData()), pcuchar(iv.constData()), encrypt)))
		return {};

	int dataSize = int(data.size());
	data.resize(data.size() + EVP_CIPHER_CTX_block_size(ctx.get()));
	int size = int(data.size());
	auto resultPointer = puchar(data.data()); //Detach only once
	if(isError(EVP_CipherUpdate(ctx.get(), resultPointer, &size, pcuchar(data.constData()), dataSize)))
		return {};

	if(!encrypt && EVP_CIPHER_mode(cipher) == EVP_CIPH_GCM_MODE)
		EVP_CIPHER_CTX_ctrl(ctx.get(), EVP_CTRL_GCM_SET_TAG, int(tag.size()), tag.data());

	int size2 = 0;
	if(isError(EVP_CipherFinal(ctx.get(), resultPointer + size, &size2)))
		return {};
	data.resize(size + size2);
	if(encrypt)
	{
		data.prepend(iv);
		if(EVP_CIPHER_mode(cipher) == EVP_CIPH_GCM_MODE)
		{
			tag = QByteArray(16, 0);
			EVP_CIPHER_CTX_ctrl(ctx.get(), EVP_CTRL_GCM_GET_TAG, int(tag.size()), tag.data());
			data.append(tag);
		}
	}
	return data;
}

QByteArray Crypto::concatKDF(QCryptographicHash::Algorithm hashAlg, quint32 keyDataLen, const QByteArray &z, const QByteArray &otherInfo)
{
	if(z.isEmpty())
		return z;
	auto hashLen = quint32(QCryptographicHash::hashLength(hashAlg));
	auto reps = quint32(std::ceil(double(keyDataLen) / double(hashLen)));
	QCryptographicHash md(hashAlg);
	QByteArray key;
	for(quint32 i = 1; i <= reps; i++)
	{
		quint32 intToFourBytes = qToBigEndian(i);
		md.reset();
		md.addData((const char*)&intToFourBytes, 4);
		md.addData(z);
		md.addData(otherInfo);
		key += md.result();
	}
	return key.left(int(keyDataLen));
}

QByteArray Crypto::curve_oid(EVP_PKEY *key)
{
	ASN1_OBJECT *obj = OBJ_nid2obj(EC_GROUP_get_curve_name(EC_KEY_get0_group(EVP_PKEY_get0_EC_KEY(key))));
	QByteArray buf(50, 0);
	buf.resize(int(OBJ_obj2txt(buf.data(), buf.size(), obj, 0)));
	return buf;
}

QByteArray Crypto::derive(EVP_PKEY *priv, EVP_PKEY *pub)
{
	if(!priv || !pub)
		return {};
	auto ctx = SCOPE(EVP_PKEY_CTX, EVP_PKEY_CTX_new(priv, nullptr));
	size_t sharedSecretLen = 0;
	if(!ctx ||
		isError(EVP_PKEY_derive_init(ctx.get())) ||
		isError(EVP_PKEY_derive_set_peer(ctx.get(), pub)) ||
		isError(EVP_PKEY_derive(ctx.get(), nullptr, &sharedSecretLen)))
		return {};
	QByteArray sharedSecret(int(sharedSecretLen), 0);
	if(isError(EVP_PKEY_derive(ctx.get(), puchar(sharedSecret.data()), &sharedSecretLen)))
		sharedSecret.clear();
	return sharedSecret;
}

QByteArray Crypto::encrypt(EVP_PKEY *pub, int padding, const QByteArray &data)
{
	auto ctx = SCOPE(EVP_PKEY_CTX, EVP_PKEY_CTX_new(pub, nullptr));
	size_t size = 0;
	if(isError(EVP_PKEY_encrypt_init(ctx.get())) ||
		isError(EVP_PKEY_CTX_set_rsa_padding(ctx.get(), padding)) ||
		isError(EVP_PKEY_encrypt(ctx.get(), nullptr, &size,
			pcuchar(data.constData()), size_t(data.size()))))
		return {};
	if(padding == RSA_PKCS1_OAEP_PADDING)
	{
		if(isError(EVP_PKEY_CTX_set_rsa_oaep_md(ctx.get(), EVP_sha256())) ||
			isError(EVP_PKEY_CTX_set_rsa_mgf1_md(ctx.get(), EVP_sha256())))
			return {};
	}
	QByteArray result(int(size), 0);
	if(isError(EVP_PKEY_encrypt(ctx.get(), puchar(result.data()), &size,
			pcuchar(data.constData()), size_t(data.size()))))
		return {};
	return result;
}

QByteArray Crypto::expand(const QByteArray &key, const QByteArray &info, int len)
{
	return hkdf(key, {}, info, len, EVP_PKEY_HKDEF_MODE_EXPAND_ONLY);
}

QByteArray Crypto::extract(const QByteArray &key, const QByteArray &salt, int len)
{
	return hkdf(key, salt, {}, len, EVP_PKEY_HKDEF_MODE_EXTRACT_ONLY);
}

std::unique_ptr<EVP_PKEY, void (*)(EVP_PKEY *)> Crypto::fromPUBKeyDer(const QByteArray &key)
{
	const auto *p = pcuchar(key.data());
	return SCOPE(EVP_PKEY, d2i_PUBKEY(nullptr, &p, long(key.length())));
}

std::unique_ptr<EVP_PKEY, void (*)(EVP_PKEY *)> Crypto::fromECPublicKeyDer(const QByteArray &key, int curveName)
{
	EVP_PKEY *params = nullptr;
	if(auto ctx = SCOPE(EVP_PKEY_CTX, EVP_PKEY_CTX_new_id(EVP_PKEY_EC, nullptr));
		!ctx ||
		isError(EVP_PKEY_paramgen_init(ctx.get())) ||
		isError(EVP_PKEY_CTX_set_ec_paramgen_curve_nid(ctx.get(), curveName)) ||
		isError(EVP_PKEY_CTX_set_ec_param_enc(ctx.get(), OPENSSL_EC_NAMED_CURVE)) ||
		isError(EVP_PKEY_paramgen(ctx.get(), &params)))
		return SCOPE(EVP_PKEY, nullptr);
	const auto *p = pcuchar(key.constData());
	return SCOPE(EVP_PKEY, d2i_PublicKey(EVP_PKEY_EC, &params, &p, long(key.length())));
}

std::unique_ptr<EVP_PKEY, void (*)(EVP_PKEY *)> Crypto::fromRSAPublicKeyDer(const QByteArray &key)
{
	const auto *p = pcuchar(key.constData());
	return SCOPE(EVP_PKEY, d2i_PublicKey(EVP_PKEY_RSA, nullptr, &p, long(key.length())));
}

std::unique_ptr<EVP_PKEY, void (*)(EVP_PKEY *)> Crypto::genECKey(EVP_PKEY *params)
{
	EVP_PKEY *key = nullptr;
	auto ctx = SCOPE(EVP_PKEY_CTX, EVP_PKEY_CTX_new(params, nullptr));
	auto result = SCOPE(EVP_PKEY, nullptr);
	if(ctx &&
		!isError(EVP_PKEY_keygen_init(ctx.get())) &&
		!isError(EVP_PKEY_keygen(ctx.get(), &key)))
		result.reset(key);
	return result;
}

QByteArray Crypto::genKey(const EVP_CIPHER *cipher)
{
#ifdef WIN32
	RAND_poll();
#else
	RAND_load_file("/dev/urandom", 1024);
#endif
	QByteArray iv(EVP_CIPHER_iv_length(cipher), 0);
	QByteArray key(EVP_CIPHER_key_length(cipher), 0);
	std::array<uchar,PKCS5_SALT_LEN> salt{};
	std::array<uchar,128> indata{};
	RAND_bytes(salt.data(), int(salt.size()));
	RAND_bytes(indata.data(), int(indata.size()));
	if(isError(EVP_BytesToKey(cipher, EVP_sha256(), salt.data(), indata.data(),
			int(indata.size()), 1, puchar(key.data()), puchar(iv.data()))))
		return {};
	return key;
}

QByteArray Crypto::hkdf(const QByteArray &key, const QByteArray &salt, const QByteArray &info, int len, int mode)
{
	auto ctx = SCOPE(EVP_PKEY_CTX, EVP_PKEY_CTX_new_id(EVP_PKEY_HKDF, nullptr));
	QByteArray out(len, 0);
	auto outlen = size_t(out.length());
	if(!ctx ||
		isError(EVP_PKEY_derive_init(ctx.get())) ||
		isError(EVP_PKEY_CTX_hkdf_mode(ctx.get(), mode)) ||
		isError(EVP_PKEY_CTX_set_hkdf_md(ctx.get(), EVP_sha256())) ||
		isError(EVP_PKEY_CTX_set1_hkdf_key(ctx.get(), pcuchar(key.data()), int(key.size()))) ||
		isError(EVP_PKEY_CTX_set1_hkdf_salt(ctx.get(), pcuchar(salt.data()), int(salt.size()))) ||
		isError(EVP_PKEY_CTX_add1_hkdf_info(ctx.get(), pcuchar(info.data()), int(info.size()))) ||
		isError(EVP_PKEY_derive(ctx.get(), puchar(out.data()), &outlen)))
		return {};
	return out;
}

bool Crypto::isError(int err)
{
	if(err < 1)
	{
		unsigned long errorCode = 0;
		while((errorCode = ERR_get_error()))
			qCWarning(CRYPTO) << ERR_error_string(errorCode, nullptr);
	}
	return err < 1;
}

QByteArray Crypto::sign_hmac(const QByteArray &key, const QByteArray &data)
{
	EVP_PKEY *pkey = EVP_PKEY_new_mac_key(EVP_PKEY_HMAC, nullptr, puchar(key.data()), int(key.size()));
	size_t req = 0;
	auto ctx = SCOPE(EVP_MD_CTX, EVP_MD_CTX_new());
	if(!ctx ||
		isError(EVP_DigestSignInit(ctx.get(), nullptr, EVP_sha256(), nullptr, pkey)) ||
		isError(EVP_DigestSignUpdate(ctx.get(), data.data(), size_t(data.length()))) ||
		isError(EVP_DigestSignFinal(ctx.get(), nullptr, &req)))
		return {};
	QByteArray sig(int(req), 0);
	if(isError(EVP_DigestSignFinal(ctx.get(), puchar(sig.data()), &req)))
		sig.clear();
	return sig;
}

QByteArray Crypto::toPublicKeyDer(EVP_PKEY *key)
{
	if(!key)
		return {};
	QByteArray der(i2d_PublicKey(key, nullptr), 0);
	auto *p = puchar(der.data());
	if(i2d_PublicKey(key, &p) != der.size())
		der.clear();
	return der;
}

QByteArray Crypto::toPublicKeyDer(const QSslKey &key)
{
	if(auto publicKey = fromPUBKeyDer(key.toDer()))
		return toPublicKeyDer(publicKey.get());
	return {};
}

QByteArray Crypto::random(int len)
{
	QByteArray out(len, 0);
	if(isError(RAND_bytes(puchar(out.data()), int(out.size()))))
		out.clear();
	return out;
}

QByteArray Crypto::xor_data(const QByteArray &a, const QByteArray &b)
{
	if(a.length() != b.length())
		return {};
	QByteArray result = a;
	for(int i = 0; i < a.length(); ++i)
		result[i] = char(a[i] ^ b[i]);
	return result;
}
