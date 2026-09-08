// SPDX-FileCopyrightText: Estonian Information System Authority
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include <QDialog>

class SslCertificate;
class QSslCertificate;

class CertificateDetails final : public QDialog
{
	Q_OBJECT

public:
	explicit CertificateDetails(const SslCertificate &c, QWidget *parent = nullptr);

	static void showCertificate(const QSslCertificate &cert, QWidget *parent, const QString &suffix = {});
};
