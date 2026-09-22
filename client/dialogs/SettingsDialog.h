// SPDX-FileCopyrightText: Estonian Information System Authority
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include <QDialog>

#include <QLabel>
#include <QVariant>

namespace digidoc { class Conf; }

namespace Ui {
class SettingsDialog;
}

class CertLabel;
class QAbstractButton;
class QLabel;
class QSslCertificate;
class SslCertificate;

class SettingsDialog final: public QDialog
{
	Q_OBJECT

public:
	enum : quint8 {
		GeneralSettings,
		SigningSettings,
		ValidationSettings,
		EncryptionSettings,
		NetworkSettings,
		DiagnosticsSettings,
		LicenseSettings
	};

	explicit SettingsDialog(int page, QWidget *parent = nullptr);
	~SettingsDialog() final;

	void showPage(int page);
	static QString certInfo(const SslCertificate &c);
	static void loadProxy( const digidoc::Conf *conf );

signals:
	void togglePrinting(bool enable);

private:
	void checkConnection();
	void retranslate(const QString& lang);
	void saveFile(const QString &name, const QString &path);
	void saveFile(const QString &name, const QByteArray &content);
	void saveProxy();
	QSslCertificate selectCert(const QString &label, const QString &format);
	void selectLanguage();
	void updateCert(const QSslCertificate &c, QPushButton *btn, CertLabel *lbl);
	void updateCDoc2Cert(const QSslCertificate &c);
	void updateSiVaCert(const QSslCertificate &c);
	void updateTSACert(const QSslCertificate &c);
	void updateDiagnostics();
	void useDefaultSettings();

	Ui::SettingsDialog *ui;
};

class CertLabel final: public QLabel {
	Q_OBJECT
public:
	using QLabel::QLabel;
	void setCert(const QSslCertificate &cert);
private:
	bool event(QEvent *event) final;
};
