// SPDX-FileCopyrightText: Estonian Information System Authority
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include <digidocpp/crypto/Signer.h>

#include <QCoreApplication>

class QAbstractButton;
class QLabel;
class QProgressBar;
class QWidget;

struct SigningProgressUI {
	QLabel *info {};
	QLabel *code {};
	QLabel *label {};
	QProgressBar *bar {};
	QAbstractButton *cancel {};
	QWidget *errorParent {};
};

class MobileProgress final: public digidoc::Signer
{
	Q_DECLARE_TR_FUNCTIONS(MobileProgress)
public:
	explicit MobileProgress(const SigningProgressUI &pui);
	~MobileProgress() final;

	bool init(const QString &ssid, const QString &cell);
	digidoc::X509Cert cert() const final;
	std::vector<unsigned char> sign(const std::string &method, const std::vector<unsigned char> &digest) const final;

private:
	class Private;
	Private *d;
};
