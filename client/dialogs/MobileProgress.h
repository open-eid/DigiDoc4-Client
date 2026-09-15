// SPDX-FileCopyrightText: Estonian Information System Authority
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include <digidocpp/crypto/Signer.h>

#include <QCoreApplication>

class QWidget;

class MobileProgress final: public digidoc::Signer
{
	Q_DECLARE_TR_FUNCTIONS(MobileProgress)
public:
	explicit MobileProgress(QWidget *parent = nullptr);
	~MobileProgress() final;

	bool init(const QString &ssid, const QString &cell);
	digidoc::X509Cert cert() const final;
	std::vector<unsigned char> sign(const std::string &method, const std::vector<unsigned char> &digest) const final;

private:
	class Private;
	Private *d;
};
