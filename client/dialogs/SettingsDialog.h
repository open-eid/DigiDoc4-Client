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

class SettingsDialog : public QDialog
{
	Q_OBJECT

public:
	explicit SettingsDialog(QWidget *parent = 0);
	~SettingsDialog();

	int exec() override;

private Q_SLOTS:
//	void on_p12Install_clicked();
//	void on_p12Remove_clicked();
//	void on_selectDefaultDir_clicked();
//	void on_showP12Cert_clicked();
	void save();


private:
    void initUI();
    void initFunctionality();
    void updateCert();
	void setProxyEnabled();
	void updateProxy();
	void loadProxy( const digidoc::Conf *conf );
	void openDirectory();
	void updateDiagnostics();
	void saveSignatureInfo(
			const QString &role,
			const QString &resolution,
			const QString &city,
			const QString &state,
			const QString &country,
			const QString &zip,
			bool force );


    void changePage(QPushButton* button);

	Ui::SettingsDialog *ui;
};
