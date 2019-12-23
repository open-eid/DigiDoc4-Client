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

namespace digidoc { class Conf; }

namespace Ui {
class SettingsDialog;
}

class QAbstractButton;

class SettingsDialog : public QDialog
{
	Q_OBJECT

public:
	enum {
		GeneralSettings,
		AccessCertSettings,
		NetworkSettings,
		DiagnosticsSettings,
		LicenseSettings
	};

	explicit SettingsDialog(QWidget *parent = nullptr, QString appletVersion = QString());
	explicit SettingsDialog(int page, QWidget *parent = nullptr, QString appletVersion = QString());
	~SettingsDialog() final;

	static void loadProxy( const digidoc::Conf *conf );

signals:
	void langChanged(const QString& lang);
	void togglePrinting(bool enable);

private:
	void changePage(QAbstractButton *button);
	void checkConnection();
	void initFunctionality();
	void installCert();
	void openDirectory();
	void retranslate(const QString& lang);
	void saveDiagnostics();
	void saveProxy();
	void selectLanguage();
	void setProxyEnabled();
	void updateCert();
	void updateProxy();
	void updateVersion();
	void updateDiagnostics();
	void useDefaultSettings();

	Ui::SettingsDialog *ui;
	QString appletVersion;
};
