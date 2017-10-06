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


#include "SettingsDialog.h"
#include "ui_SettingsDialog.h"
#include "dialogs/CertificateDetails.h"
#include "effects/Overlay.h"
#include "Styles.h"

SettingsDialog::SettingsDialog(QWidget *parent) :
	QDialog(parent),
	ui(new Ui::SettingsDialog)
{
	ui->setupUi(this);
	setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint);
	setWindowModality(Qt::ApplicationModal);

	QFont headerFont = Styles::font(Styles::Regular, 18, QFont::Bold);
	QFont regularFont = Styles::font(Styles::Regular, 14);
	QFont condensed12 = Styles::font(Styles::Condensed, 12);

	// Menu
	ui->lblMenuSettings->setFont(headerFont);
	ui->btnMenuGeneral->setFont(condensed12);
	ui->btnMenuSigning->setFont(condensed12);
	ui->btnMenuCertificate->setFont(condensed12);
	ui->btnMenuProxy->setFont(condensed12);
	ui->btnMenuDiagnostics->setFont(condensed12);
	ui->btnMenuInfo->setFont(condensed12);

	// pageGeneral
	ui->lblGeneralLang->setFont(headerFont);
	ui->lblGeneralDirectory->setFont(headerFont);
	ui->lblGeneralCheckUpdatePeriod->setFont(headerFont);

	ui->rdGeneralEstonian->setFont(regularFont);
	ui->rdGeneralRussian->setFont(regularFont);
	ui->rdGeneralEnglish->setFont(regularFont);

	ui->rdGeneralSameDirectory->setFont(regularFont);
	ui->rdGeneralSpecifyDirectory->setFont(regularFont);
	ui->btGeneralChooseDirectory->setFont(regularFont);
	ui->txtGeneralDirectory->setFont(regularFont);

	ui->cmbGeneralCheckUpdatePeriod->setFont(regularFont);
	ui->chkGeneralTslRefresh->setFont(regularFont);

	ui->cmbGeneralCheckUpdatePeriod->addItem("Kord päevas");
	ui->cmbGeneralCheckUpdatePeriod->addItem("Kord nädalas");
	ui->cmbGeneralCheckUpdatePeriod->addItem("Kord kuus");


	// pageSigning
	ui->lblSigningFileType->setFont(headerFont);
	ui->lblSigningRole->setFont(headerFont);
	ui->lblSigningAddress->setFont(headerFont);

	ui->rdSigningAsice->setFont(regularFont);
	ui->rdSigningBdoc->setFont(regularFont);
	ui->lblSigningExplane->setFont(regularFont);
	ui->lblSigningCity->setFont(regularFont);
	ui->lblSigningCounty->setFont(regularFont);
	ui->lblSigningCountry->setFont(regularFont);
	ui->lblSigningZipCode->setFont(regularFont);
	ui->txtSigningCity->setFont(regularFont);
	ui->txtSigningCounty->setFont(regularFont);
	ui->txtSigningCountry->setFont(regularFont);
	ui->txtSigningZipCode->setFont(regularFont);

	// pageAccessSert
	ui->txtEvidence->setFont(regularFont);
	ui->chkEvidenceIgnore->setFont(regularFont);

	// pageProxy
	ui->rdProxyNone->setFont(regularFont);
	ui->rdProxySystem->setFont(regularFont);
	ui->rdProxyManual->setFont(regularFont);

	ui->lblProxyHost->setFont(regularFont);
	ui->lblProxyPort->setFont(regularFont);
	ui->lblProxyUsername->setFont(regularFont);
	ui->lblProxyPassword->setFont(regularFont);
	ui->txtProxyHost->setFont(regularFont);
	ui->txtProxyPort->setFont(regularFont);
	ui->txtProxyUsername->setFont(regularFont);
	ui->txtProxyPassword->setFont(regularFont);

	// pageDiagnostics
	ui->txtDiagnostics->setFont(regularFont);

	// pageInfo
	ui->txtInfo->setFont(regularFont);

	// navigationArea
	ui->txtNavVersion->setFont(Styles::font( Styles::Regular, 12 ));
	ui->btNavFromFile->setFont(condensed12);
	ui->btnNavFromHistory->setFont(condensed12);

	ui->btnNavUseByDefault->setFont(condensed12);
	ui->btnNavInstallManually->setFont(condensed12);
	ui->btnNavShowCertificate->setFont(condensed12);

	ui->btNavClose->setFont(Styles::font( Styles::Condensed, 14 ));

	changePage(ui->btnMenuGeneral);


	connect( ui->btNavClose, &QPushButton::clicked, this, &SettingsDialog::accept );
	connect( this, &SettingsDialog::finished, this, &SettingsDialog::close );

	connect( ui->btnNavShowCertificate, &QPushButton::clicked, this,
			 [this]()
		{
			CertificateDetails dlg(this);
			dlg.exec();
		}
			);


	connect( ui->btnMenuGeneral,  &QPushButton::clicked, this, [this](){ changePage(ui->btnMenuGeneral); ui->stackedWidget->setCurrentIndex(0); } );
	connect( ui->btnMenuSigning, &QPushButton::clicked, this, [this](){ changePage(ui->btnMenuSigning); ui->stackedWidget->setCurrentIndex(1); } );
	connect( ui->btnMenuCertificate, &QPushButton::clicked, this, [this](){ changePage(ui->btnMenuCertificate); ui->stackedWidget->setCurrentIndex(2); } );
	connect( ui->btnMenuProxy, &QPushButton::clicked, this, [this](){ changePage(ui->btnMenuProxy); ui->stackedWidget->setCurrentIndex(3); } );
	connect( ui->btnMenuDiagnostics, &QPushButton::clicked, this, [this](){ changePage(ui->btnMenuDiagnostics); ui->stackedWidget->setCurrentIndex(4); } );
	connect( ui->btnMenuInfo, &QPushButton::clicked, this, [this](){ changePage(ui->btnMenuInfo); ui->stackedWidget->setCurrentIndex(5); } );
}

SettingsDialog::~SettingsDialog()
{
	delete ui;
}


void SettingsDialog::changePage(QPushButton* button)
{
	if(button->isChecked())
	{
		ui->btnMenuGeneral->setChecked(false);
		ui->btnMenuSigning->setChecked(false);
		ui->btnMenuCertificate->setChecked(false);
		ui->btnMenuProxy->setChecked(false);
		ui->btnMenuDiagnostics->setChecked(false);
		ui->btnMenuInfo->setChecked(false);
	}

	button->setChecked(true);

	if(button == ui->btnMenuCertificate)
	{
		ui->btNavFromFile->hide();
		ui->btnNavFromHistory->hide();

		ui->btnNavUseByDefault->show();
		ui->btnNavInstallManually->show();
		ui->btnNavShowCertificate->show();
	}
	else if(button == ui->btnMenuGeneral)
	{
		ui->btNavFromFile->show();
		ui->btnNavFromHistory->show();

		ui->btnNavUseByDefault->hide();
		ui->btnNavInstallManually->hide();
		ui->btnNavShowCertificate->hide();
	}
	else
	{
		ui->btNavFromFile->hide();
		ui->btnNavFromHistory->hide();

		ui->btnNavUseByDefault->hide();
		ui->btnNavInstallManually->hide();
		ui->btnNavShowCertificate->hide();
	}

}

int SettingsDialog::exec()
{
	Overlay overlay( parentWidget() );
	overlay.show();
	auto rc = QDialog::exec();
	overlay.close();

	return rc;
}
