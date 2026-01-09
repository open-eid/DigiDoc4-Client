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

#include <QtCore/QtEndian>

#include <cdoc/utils/memory.h>

#include <openssl/core_names.h>
#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/kdf.h>
#include <openssl/rand.h>

#include <array>
#include <cmath>

using puchar = uchar *;
using pcuchar = const uchar *;

Q_LOGGING_CATEGORY(CRYPTO,"CRYPTO")

QByteArray Crypto::concatKDF(QCryptographicHash::Algorithm hashAlg, const QByteArray &z, const QByteArray &otherInfo)
{
	if(z.isEmpty())
		return z;
	quint32 keyDataLen = 32;
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
	QByteArray buf(50, 0);
	std::array<char, 64> group{};
	size_t size = group.size();
	if(EVP_PKEY_get_utf8_string_param(key, OSSL_PKEY_PARAM_GROUP_NAME, group.data(), group.size(), &size) != 1)
	{
		buf.clear();
		return buf;
	}
	ASN1_OBJECT *obj = OBJ_nid2obj(OBJ_sn2nid(group.data()));
	if(int size = OBJ_obj2txt(buf.data(), buf.size(), obj, 1); size != NID_undef)
		buf.resize(size);
	else
		buf.clear();
	return buf;
}

QByteArray Crypto::extract(const QByteArray &key, const QByteArray &salt, int len)
{
	return hkdf(key, salt, {}, len, EVP_PKEY_HKDEF_MODE_EXTRACT_ONLY);
}

QByteArray Crypto::hkdf(const QByteArray &key, const QByteArray &salt, const QByteArray &info, int len, int mode)
{
	auto ctx = libcdoc::make_unique_ptr<EVP_PKEY_CTX_free>(EVP_PKEY_CTX_new_id(EVP_PKEY_HKDF, nullptr));
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

QByteArray Crypto::random(int len)
{
	QByteArray out(len, 0);
	if(isError(RAND_bytes(puchar(out.data()), int(out.size()))))
		out.clear();
	return out;
}
