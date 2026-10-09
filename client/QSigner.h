// SPDX-FileCopyrightText: Estonian Information System Authority
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "TokenData.h"

#include <digidocpp/crypto/Signer.h>

#include <QtCore/QCoreApplication>
#include <QtCore/QCryptographicHash>

class QSmartCard;
class TokenData;

class QSigner final : public digidoc::Signer
{
	Q_DECLARE_TR_FUNCTIONS(QSigner);
public:
	explicit QSigner(const TokenData &token);

	digidoc::X509Cert cert() const final;
	std::vector<unsigned char> sign(const std::string &method,
		const std::vector<unsigned char> &digest) const final;

private:
	static QCryptographicHash::Algorithm methodToNID(const std::string &method);

	TokenData m_token;
};
