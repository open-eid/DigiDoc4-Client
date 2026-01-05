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

#pragma once

#include <QtCore/QCryptographicHash>
#include <QtCore/QLoggingCategory>

using EVP_PKEY = struct evp_pkey_st;

Q_DECLARE_LOGGING_CATEGORY(CRYPTO)

class Crypto
{
public:
	static QByteArray curve_oid(EVP_PKEY *key);
	static QByteArray concatKDF(QCryptographicHash::Algorithm digestMethod, const QByteArray &z, const QByteArray &otherInfo);
	static QByteArray extract(const QByteArray &key, const QByteArray &salt, int len = 32);
	static QByteArray random(int len = 32);

private:
	static QByteArray hkdf(const QByteArray &key, const QByteArray &salt, const QByteArray &info, int len = 32, int mode = 0);
	static bool isError(int err);
};
