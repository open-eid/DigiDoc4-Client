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

#include "Application.h"
#include "AccessCert.h"
#include "Styles.h"

#include "common/Settings.h"
#include "common/SslCertificate.h"
#include "FileDialog.h"
#include "dialogs/CertificateDetails.h"
#include "effects/Overlay.h"

#include <digidocpp/Conf.h>
#include <QDebug>
#include <QtNetwork/QSslCertificate>
#include <QtNetwork/QNetworkProxy>

SettingsDialog::SettingsDialog(QWidget *parent) :
	QDialog(parent),
	ui(new Ui::SettingsDialog)
{
    initUI();
    initFunctionality();
}

SettingsDialog::~SettingsDialog()
{
    delete ui;
}

void SettingsDialog::initUI()
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
    ui->txtEvidenceCert->setFont(regularFont);
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
			CertificateDetails dlg(QSslCertificate(), this);
			dlg.exec();
		}
			);


	connect( ui->btnMenuGeneral,  &QPushButton::clicked, this, [this](){ changePage(ui->btnMenuGeneral); ui->stackedWidget->setCurrentIndex(0); } );
	connect( ui->btnMenuSigning, &QPushButton::clicked, this, [this](){ changePage(ui->btnMenuSigning); ui->stackedWidget->setCurrentIndex(1); } );
	connect( ui->btnMenuCertificate, &QPushButton::clicked, this, [this](){ changePage(ui->btnMenuCertificate); ui->stackedWidget->setCurrentIndex(2); } );
	connect( ui->btnMenuProxy, &QPushButton::clicked, this, [this](){ changePage(ui->btnMenuProxy); ui->stackedWidget->setCurrentIndex(3); } );
	connect( ui->btnMenuDiagnostics, &QPushButton::clicked, this, [this](){ changePage(ui->btnMenuDiagnostics); ui->stackedWidget->setCurrentIndex(4); } );
	connect( ui->btnMenuInfo, &QPushButton::clicked, this, [this](){ changePage(ui->btnMenuInfo); ui->stackedWidget->setCurrentIndex(5); } );

	connect( this, &SettingsDialog::finished, this, &SettingsDialog::save );

	connect( ui->btGeneralChooseDirectory, &QPushButton::clicked, this, &SettingsDialog::openDirectory );
}

void SettingsDialog::initFunctionality()
{
	//languages->setCurrentIndex( lang.indexOf( Settings::language() ) );

	if(Settings::language() == "en")
	{
		ui->rdGeneralEnglish->setChecked(true);
	}
	else if(Settings::language() == "ru")
	{
		ui->rdGeneralRussian->setChecked(true);
	}
	else
	{
		ui->rdGeneralEstonian->setChecked(true);
	}

	connect( ui->rdGeneralEstonian, &QRadioButton::toggled, this, [this](bool checked) { if(checked) { Settings().setValueEx( "Main/Language", "et", "et" ); } } );
	connect( ui->rdGeneralEnglish, &QRadioButton::toggled, this, [this](bool checked) { if(checked) { Settings().setValueEx( "Main/Language", "en", "et" ); } } );
	connect( ui->rdGeneralRussian, &QRadioButton::toggled, this, [this](bool checked) { if(checked) { Settings().setValueEx( "Main/Language", "ru", "et" ); } } );


/* Ei esine uues dialoogis */
//    d->showIntro->setChecked( Settings(qApp->applicationName()).value( "ClientIntro", true ).toBool() );
//	d->showIntro2->setChecked( Settings(qApp->applicationName()).value( "Crypto/Intro", true ).toBool() );
//	d->cdocwithddoc->setChecked( Settings(qApp->applicationName()).value( "cdocwithddoc", false ).toBool() );
//	connect(d->cdocwithddoc, &QCheckBox::toggled, [](bool checked){
//	Settings(qApp->applicationName()).setValueEx( "cdocwithddoc", checked, false );
//	});
	Settings(qApp->applicationName()).beginGroup( "Client" );
	// Cleanup old keys
	Settings(qApp->applicationName()).remove( "lastPath" );
	Settings(qApp->applicationName()).remove( "type" );
	Settings(qApp->applicationName()).remove( "Intro" );
	Settings(qApp->applicationName()).remove( "Intro" );
	updateCert();
#ifdef Q_OS_MAC
//	d->p12Label->setText( tr(
//		"Regarding to terms and conditions of validity confirmation service you're "
//		"allowed to use the service in extent of 10 signatures per month. Additional "
//		"information is available from <a href=\"http://www.id.ee/kehtivuskinnitus\">"
//		"http://www.id.ee/kehtivuskinnitus</a> or phone 1777 (only from Estonia), (+372) 6773377") );
//	d->label->hide();
	ui->rdGeneralSameDirectory->hide();
	ui->txtGeneralDirectory->hide();
//	d->selectDefaultDir->hide();
	ui->rdGeneralSpecifyDirectory->hide();
#else
	if(Settings(qApp->applicationName()).value( "DefaultDir" ).isNull())
	{
		ui->rdGeneralSameDirectory->setChecked( true );
		ui->btGeneralChooseDirectory->setEnabled(false);
	}
	else
	{
		ui->rdGeneralSpecifyDirectory->setChecked( true );
		ui->btGeneralChooseDirectory->setEnabled(true);
	}
	connect( ui->rdGeneralSameDirectory, &QRadioButton::toggled, this, [this](bool checked)
		{
			if(checked)
			{
				ui->btGeneralChooseDirectory->setEnabled(false);
				ui->txtGeneralDirectory->setText( QString() );
		} } );
	connect( ui->rdGeneralSpecifyDirectory, &QRadioButton::toggled, this, [this](bool checked)
		{
			if(checked)
			{
				ui->btGeneralChooseDirectory->setEnabled(true);
		} } );
	ui->txtGeneralDirectory->setText( Settings(qApp->applicationName()).value( "DefaultDir" ).toString() );
#endif

	if(Settings(qApp->applicationName()).value( "type", "bdoc" ).toString() == "bdoc")
		ui->rdSigningBdoc->setChecked(true);
	else
		ui->rdSigningAsice->setChecked(true);
	connect( ui->rdSigningBdoc, &QRadioButton::toggled, this, [this](bool checked) { if(checked) { Settings(qApp->applicationName()).setValueEx( "type", "bdoc", "bdoc" ); } } );
	connect( ui->rdSigningAsice, &QRadioButton::toggled, this, [this](bool checked) { if(checked) { Settings(qApp->applicationName()).setValueEx( "type", "asice", "bdoc" ); } } );

	ui->chkGeneralTslRefresh->setChecked( qApp->confValue( Application::TSLOnlineDigest ).toBool() );
	connect( ui->chkGeneralTslRefresh, &QCheckBox::toggled, []( bool checked ) { qApp->setConfValue( Application::TSLOnlineDigest, checked ); } );

/* Ei esine uues dialoogis */
//	d->tokenRestart->hide();
//#ifdef Q_OS_WIN
//	connect( d->tokenRestart, &QPushButton::clicked, []{
//		qApp->setProperty("restart", true);
//		qApp->quit();
//	});
//	d->tokenBackend->setChecked(Settings(qApp->applicationName()).value("tokenBackend").toUInt());
//	connect( d->tokenBackend, &QCheckBox::toggled, [=](bool checked){
//		Settings(qApp->applicationName()).setValueEx("tokenBackend", int(checked), 0);
//		d->tokenRestart->show();
//	});
//#else
//	d->tokenBackend->hide();
//#endif


	ui->txtSigningRole->setText( Settings(qApp->applicationName()).value( "Role" ).toString() );
//	ui->signResolutionInput->setText( Settings(qApp->applicationName()).value( "Resolution" ).toString() );
	ui->txtSigningCity->setText( Settings(qApp->applicationName()).value( "City" ).toString() );
	ui->txtSigningCounty->setText( Settings(qApp->applicationName()).value( "State" ).toString() );
	ui->txtSigningCountry->setText( Settings(qApp->applicationName()).value( "Country" ).toString() );
	ui->txtSigningZipCode->setText( Settings(qApp->applicationName()).value( "Zip" ).toString() );


//	d->signOverwrite->setChecked( s.value( "Overwrite", false ).toBool() );

	connect( ui->rdProxyNone, &QRadioButton::toggled, this, &SettingsDialog::setProxyEnabled );
	connect( ui->rdProxySystem, &QRadioButton::toggled, this, &SettingsDialog::setProxyEnabled );
	connect( ui->rdProxyManual, &QRadioButton::toggled, this, &SettingsDialog::setProxyEnabled );
	switch(Settings(qApp->applicationName()).value("proxyConfig", 0 ).toInt())
	{
	case 1:
		ui->rdProxySystem->setChecked( true );
		break;
	case 2:
		ui->rdProxyManual->setChecked( true );
		break;
	default:
		ui->rdProxyNone->setChecked( true );
		break;
	}

	updateProxy();
	ui->chkEvidenceIgnore->setChecked( Application::confValue( Application::PKCS12Disable, false ).toBool() );

	Settings(qApp->applicationName()).endGroup();
}


void SettingsDialog::updateCert()
{
    QSslCertificate c = AccessCert::cert();
    if( !c.isNull() )
        ui->txtEvidenceCert->setText(
            tr("Issued to: %1<br />Valid to: %2 %3")
            .arg( SslCertificate(c).subjectInfo( QSslCertificate::CommonName ) )
            .arg( c.expiryDate().toString("dd.MM.yyyy") )
            .arg( !SslCertificate(c).isValid() ? "<font color='red'>(" + tr("expired") + ")</font>" : "" ) );
    else
        ui->txtEvidenceCert->setText( "<b>" + tr("Server access certificate is not installed.") + "</b>" );
    ui->btnNavShowCertificate->setEnabled( !c.isNull() );
    ui->btnNavShowCertificate->setProperty( "cert", QVariant::fromValue( c ) );
}



void SettingsDialog::setProxyEnabled()
{
	ui->txtProxyHost->setEnabled(ui->rdProxyManual->isChecked());
	ui->txtProxyPort->setEnabled(ui->rdProxyManual->isChecked());
	ui->txtProxyUsername->setEnabled(ui->rdProxyManual->isChecked());
	ui->txtProxyPassword->setEnabled(ui->rdProxyManual->isChecked());
}

void SettingsDialog::updateProxy()
{
	ui->txtProxyHost->setText(Application::confValue( Application::ProxyHost ).toString());
	ui->txtProxyPort->setText(Application::confValue( Application::ProxyPort ).toString());
	ui->txtProxyUsername->setText(Application::confValue( Application::ProxyUser ).toString());
	ui->txtProxyPassword->setText(Application::confValue( Application::ProxyPass ).toString());
//	d->proxySSL->setChecked(Application::confValue( Application::ProxySSL ).toBool());
}

void SettingsDialog::save()
{
//	s.setValueEx( "Crypto/Intro", d->showIntro2->isChecked(), true );
//	Settings(qApp->applicationName()).setValue( "ClientIntro", d->showIntro->isChecked() );
	Settings(qApp->applicationName()).beginGroup( "Client" );
//	Settings(qApp->applicationName()).setValueEx( "Overwrite", d->signOverwrite->isChecked(), false );
#ifndef Q_OS_MAC
//	s.setValueEx( "AskSaveAs", d->askSaveAs->isChecked(), true );
	Settings(qApp->applicationName()).setValueEx("DefaultDir", ui->txtGeneralDirectory->text(), QString());
#endif

	if(ui->rdProxyNone->isChecked())
	{
		Settings(qApp->applicationName()).setValueEx("proxyConfig", 0, 0);
	}
	else if(ui->rdProxySystem->isChecked())
	{
		Settings(qApp->applicationName()).setValueEx("proxyConfig", 1, 0);
	}
	else if(ui->rdProxyManual->isChecked())
	{
		Settings(qApp->applicationName()).setValueEx("proxyConfig", 2, 0);
	}

	Application::setConfValue( Application::ProxyHost, ui->txtProxyHost->text() );
	Application::setConfValue( Application::ProxyPort, ui->txtProxyPort->text() );
	Application::setConfValue( Application::ProxyUser, ui->txtProxyUsername->text() );
	Application::setConfValue( Application::ProxyPass, ui->txtProxyPassword->text() );
//	Application::setConfValue( Application::ProxySSL, d->proxySSL->isChecked() );
	Application::setConfValue( Application::PKCS12Disable, ui->chkEvidenceIgnore->isChecked() );
	loadProxy(digidoc::Conf::instance());
	updateProxy();

	saveSignatureInfo(
		ui->txtSigningRole->text(),
		QString(), //d->signResolutionInput->text(),
		ui->txtSigningCity->text(),
		ui->txtSigningCounty->text(),
		ui->txtSigningCountry->text(),
		ui->txtSigningZipCode->text(),
		true );
}

void SettingsDialog::loadProxy( const digidoc::Conf *conf )
{
	switch(Settings(qApp->applicationName()).value("proxyConfig", 0).toUInt())
	{
	case 0:
		QNetworkProxyFactory::setUseSystemConfiguration(false);
		QNetworkProxy::setApplicationProxy(QNetworkProxy());
		break;
	case 1:
		QNetworkProxyFactory::setUseSystemConfiguration(true);
		break;
	default:
		QNetworkProxyFactory::setUseSystemConfiguration(false);
		QNetworkProxy::setApplicationProxy(QNetworkProxy(QNetworkProxy::HttpProxy,
			QString::fromStdString(conf->proxyHost()),
			QString::fromStdString(conf->proxyPort()).toUInt(),
			QString::fromStdString(conf->proxyUser()),
			QString::fromStdString(conf->proxyPass())));
		break;
	}
}


void SettingsDialog::openDirectory()
{
	QString dir = Settings().value( "Client/DefaultDir" ).toString();
	dir = FileDialog::getExistingDirectory( this, tr("Select folder"), dir );

	if(!dir.isEmpty())
	{
		ui->rdGeneralSpecifyDirectory->setChecked( true );
		Settings().setValueEx( "Client/DefaultDir", dir, QString() );
		ui->txtGeneralDirectory->setText( dir );
	}
}


void SettingsDialog::saveSignatureInfo(
		const QString &role,
		const QString &resolution,
		const QString &city,
		const QString &state,
		const QString &country,
		const QString &zip,
		bool force )
{
	Settings(qApp->applicationName()).beginGroup( "Client" );
	if( force || Settings(qApp->applicationName()).value( "Overwrite", "false" ).toBool() )
	{
		Settings(qApp->applicationName()).setValueEx( "Role", role, QString() );
		Settings(qApp->applicationName()).setValueEx( "Resolution", resolution, QString() );
		Settings(qApp->applicationName()).setValueEx( "City", city, QString() );
		Settings(qApp->applicationName()).setValueEx( "State", state, QString() ),
		Settings(qApp->applicationName()).setValueEx( "Country", country, QString() );
		Settings(qApp->applicationName()).setValueEx( "Zip", zip, QString() );
	}
	Settings(qApp->applicationName()).endGroup();
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
