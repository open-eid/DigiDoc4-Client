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

#include <QVariant>

namespace digidoc { class Conf; }

namespace Ui {
class SettingsDialog;
}

class QAbstractButton;

class SettingsDialog final: public QDialog
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

	explicit SettingsDialog(QWidget *parent = nullptr);
	explicit SettingsDialog(int page, QWidget *parent = nullptr);
	~SettingsDialog() final;

	void showPage(int page);
	static void loadProxy( const digidoc::Conf *conf );
	static void setValueEx(const QString &key, const QVariant &value, const QVariant &def = {});

signals:
	void langChanged(const QString& lang);
	void togglePrinting(bool enable);

private:
	void changePage(QAbstractButton *button);
	void checkConnection();
	void initFunctionality();
	void installCert();
	void retranslate(const QString& lang);
	void saveFile(const QString &name, const QByteArray &content);
	void saveProxy();
	void selectLanguage();
	void setProxyEnabled();
	void updateCert();
	void updateProxy();
	void updateVersion();
	void updateDiagnostics();
	void useDefaultSettings();

	Ui::SettingsDialog *ui;
};
