/*
 * QDigiDoc4
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
	void langChanged(const QString& lang);
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
	void updateVersion();
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
