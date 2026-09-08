// SPDX-FileCopyrightText: Estonian Information System Authority
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "MobileProgress.h"

#include <digidocpp/crypto/Signer.h>

#include <QCoreApplication>

class SmartIDProgress final: public digidoc::Signer
{
	Q_DECLARE_TR_FUNCTIONS(MobileProgress)
public:
	explicit SmartIDProgress(const SigningProgressUI &pui);
	~SmartIDProgress() final;
	digidoc::X509Cert cert() const final;
	bool init(const QString &country, const QString &idCode, const QString &fileName);
	std::vector<unsigned char> sign(const std::string &method, const std::vector<unsigned char> &digest) const final;

private:
	class Private;
	Private *d;
};
