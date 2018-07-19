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
#include "CheckConnection.h"
#include "Colors.h"
#include "FileDialog.h"
#include "Styles.h"
#include "dialogs/CertificateDetails.h"
#include "dialogs/FirstRun.h"
#include "effects/Overlay.h"
#include "effects/FadeInNotification.h"
#include "util/CertUtil.h"

#include "common/Configuration.h"
#include "common/Diagnostics.h"
#include "common/Settings.h"
#include "common/SslCertificate.h"

#include <digidocpp/Conf.h>

#include <QFile>
#include <QDesktopServices>
#include <QInputDialog>
#include <QIODevice>
#include <QStandardPaths>
#include <QSvgWidget>
#include <QSysInfo>
#include <QTextBrowser>
#include <QThread>
#include <QThreadPool>
#include <QUrl>
#include <QtCore/QJsonObject>
#include <QtNetwork/QNetworkProxy>
#include <QtNetwork/QSslCertificate>

#ifdef Q_OS_WIN
#include <qt_windows.h>
#include <QProcess>
#endif

SettingsDialog::SettingsDialog(QWidget *parent, QString appletVersion)
: QDialog(parent)
, ui(new Ui::SettingsDialog)
, appletVersion(appletVersion)
{
	initUI();
	initFunctionality();
}

SettingsDialog::SettingsDialog(int page, QWidget *parent, QString appletVersion)
: SettingsDialog(parent, appletVersion)
{
	ui->stackedWidget->setCurrentIndex(page);

	if(page != GeneralSettings)
	{
		ui->btnMenuGeneral->setChecked(false);
		switch(page)
		{
		case SigningSettings:
			changePage(ui->btnMenuSigning);
			break;
		case AccessCertSettings:
			changePage(ui->btnMenuCertificate);
			break;
		case NetworkSettings:
			changePage(ui->btnMenuProxy);
			break;
		case DiagnosticsSettings:
			changePage(ui->btnMenuDiagnostics);
			break;
		case LicenseSettings:
			changePage(ui->btnMenuInfo);
			break;
		}
	}
}


SettingsDialog::~SettingsDialog()
{
	QApplication::restoreOverrideCursor();
	delete ui;
}

void SettingsDialog::initUI()
{
	ui->setupUi(this);
#ifdef Q_OS_MAC
	setWindowFlags(Qt::Dialog | Qt::WindowTitleHint | Qt::WindowSystemMenuHint | Qt::WindowCloseButtonHint);
	setWindowModality(Qt::WindowModal);
#else
	setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint);
	setWindowModality(Qt::ApplicationModal);
#endif 

	QFont headerFont = Styles::font(Styles::Regular, 18, QFont::Bold);
	QFont regularFont = Styles::font(Styles::Regular, 14);
	QFont condensed12 = Styles::font(Styles::Condensed, 12);

	// Menu
	ui->lblMenuSettings->setFont(headerFont);
	ui->btnMenuGeneral->setFont(condensed12);
	ui->btnMenuSigning->setFont(condensed12);
	ui->btnMenuCertificate->setFont(condensed12);
	ui->btnMenuCertificate->setText(tr("ACCESS CERTIFICATE",
#ifdef Q_OS_WIN
		"win"
#else
		"not-win"
#endif
	));
	ui->btnMenuProxy->setFont(condensed12);
	ui->btnMenuDiagnostics->setFont(condensed12);
	ui->btnMenuInfo->setFont(condensed12);

	// pageGeneral
	ui->lblGeneralLang->setFont(headerFont);
	ui->lblDefaultDirectory->setFont(headerFont);
	ui->lblGeneralCheckUpdatePeriod->setFont(headerFont);

	ui->rdGeneralEstonian->setFont(regularFont);
	ui->rdGeneralRussian->setFont(regularFont);
	ui->rdGeneralEnglish->setFont(regularFont);

	ui->rdGeneralSameDirectory->setFont(regularFont);
	ui->rdGeneralSpecifyDirectory->setFont(regularFont);
	ui->btGeneralChooseDirectory->setFont(regularFont);
	ui->txtGeneralDirectory->setFont(regularFont);

#ifdef Q_OS_WIN
	ui->cmbGeneralCheckUpdatePeriod->setFont(regularFont);
#else
	ui->lblGeneralCheckUpdatePeriod->hide();
	ui->cmbGeneralCheckUpdatePeriod->hide();
#endif
	ui->chkGeneralTslRefresh->setFont(regularFont);

	// pageSigning
	ui->lblSigningFileType->setFont(headerFont);
	ui->lblSigningRole->setFont(headerFont);
	ui->lblSigningAddress->setFont(headerFont);
	ui->chkShowPrintSummary->setFont(regularFont);

	ui->rdSigningAsice->setFont(regularFont);
	ui->rdSigningBdoc->setFont(regularFont);
	ui->lblSigningExplained->setFont(regularFont);
	ui->lblSigningCity->setFont(regularFont);
	ui->lblSigningCounty->setFont(regularFont);
	ui->lblSigningCountry->setFont(regularFont);
	ui->lblSigningZipCode->setFont(regularFont);
	ui->txtSigningRole->setFont(regularFont);
	ui->txtSigningCity->setFont(regularFont);
	ui->txtSigningCounty->setFont(regularFont);
	ui->txtSigningCountry->setFont(regularFont);
	ui->txtSigningZipCode->setFont(regularFont);

	// pageAccessSert
	ui->txtAccessCert->setFont(regularFont);
	ui->chkIgnoreAccessCert->setFont(regularFont);

	// pageProxy
	ui->rdProxyNone->setFont(regularFont);
	ui->rdProxySystem->setFont(regularFont);
	ui->rdProxyManual->setFont(regularFont);

	ui->lblProxyHost->setFont(regularFont);
	ui->lblProxyPort->setFont(regularFont);
	ui->lblProxyUsername->setFont(regularFont);
	ui->lblProxyPassword->setFont(regularFont);
	ui->chkProxyEnableForSSL->setFont(regularFont);
	ui->txtProxyHost->setFont(regularFont);
	ui->txtProxyPort->setFont(regularFont);
	ui->txtProxyUsername->setFont(regularFont);
	ui->txtProxyPassword->setFont(regularFont);

	// pageDiagnostics
	QSvgWidget* structureFunds = new QSvgWidget(":/images/Struktuurifondid.svg", ui->structureFunds);
	structureFunds->resize(ui->structureFunds->width(), ui->structureFunds->height());
	structureFunds->show();
	ui->contact->setFont(regularFont);

	ui->txtDiagnostics->setFont(regularFont);

	// pageInfo
	ui->txtInfo->setFont(regularFont);

	// navigationArea
	ui->txtNavVersion->setFont(Styles::font( Styles::Regular, 12 ));
	ui->btAppStore->setFont(condensed12);
	ui->btnNavFromHistory->setFont(condensed12);

	ui->btnNavUseByDefault->setFont(condensed12);
	ui->btnNavInstallManually->setFont(condensed12);
	ui->btnNavShowCertificate->setFont(condensed12);
	ui->btnNavShowCertificate->setText(tr("SHOW CERTIFICATE",
#ifdef Q_OS_WIN
		"win"
#else
		"not-win"
#endif
	));
	ui->btnFirstRun->setFont(condensed12);
	ui->btnNavSaveReport->setFont(condensed12);
	ui->btnCheckConnection->setFont(condensed12);

	ui->btNavClose->setFont(Styles::font( Styles::Condensed, 14 ));

	changePage(ui->btnMenuGeneral);

	QString package;
#ifndef Q_OS_MAC
	QStringList packages = Common::packages({
		"Eesti ID-kaardi tarkvara", "Estonian ID-card software", "estonianidcard", "eID software"});
	if( !packages.isEmpty() )
		package = "<br />" + tr("Base version:") + " " + packages.first();
#endif
	ui->txtNavVersion->setText( tr("%1 version %2, released %3%4")
		.arg( tr("DigiDoc4 client"), qApp->applicationVersion(), BUILD_DATE, package ) );

	connect(&Configuration::instance(), &Configuration::finished, this, [=](bool /*update*/, const QString &error){
		if(error.isEmpty())
		{
			ui->btAppStore->setVisible(ui->stackedWidget->currentIndex() == GeneralSettings && hasNewerVersion());
			return;
		}
		QMessageBox b(QMessageBox::Warning, tr("Checking updates has failed."),
			tr("Checking updates has failed.") + "<br />" + tr("Please try again."),
			QMessageBox::Ok, this);
		b.setTextFormat(Qt::RichText);
		b.setDetailedText(error);
		b.exec();
	});
	connect(ui->btAppStore, &QPushButton::clicked, []{
#if defined(Q_OS_WIN)
		QDesktopServices::openUrl(QUrl(tr("https://installer.id.ee/?lang=eng")));
#elif defined(Q_OS_MAC)
		QDesktopServices::openUrl(QUrl("https://itunes.apple.com/us/app/digidoc4-client/id1370791134?ls=1&mt=12"));
#else
		QDesktopServices::openUrl(QUrl(tr("https://installer.id.ee/?lang=eng&os=linux")));
#endif
	});


	connect( ui->btNavClose, &QPushButton::clicked, this, &SettingsDialog::accept );
	connect( this, &SettingsDialog::finished, this, &SettingsDialog::close );

	connect(ui->btnCheckConnection, &QPushButton::clicked, this, &SettingsDialog::checkConnection);
	connect( ui->btnNavShowCertificate, &QPushButton::clicked, this,
			 [this]()
		{
			CertUtil::showCertificate(SslCertificate(AccessCert::cert()), this);
		}
			);
	connect(ui->btnFirstRun, &QPushButton::clicked, this,
			 [this]()
		{
			FirstRun dlg(this);

			connect(&dlg, &FirstRun::langChanged, this,
					[this](const QString& lang )
					{
						retranslate(lang);
						selectLanguage();
					}
			);
			dlg.exec();
		}
			);
	connect( ui->btnNavInstallManually, &QPushButton::clicked, this, &SettingsDialog::installCert );
	connect( ui->btnNavUseByDefault, &QPushButton::clicked, this, &SettingsDialog::removeCert );
	connect( ui->btnNavSaveReport, &QPushButton::clicked, this, &SettingsDialog::saveDiagnostics );
	connect( ui->btnNavFromHistory, &QPushButton::clicked, this,  [this](){ emit removeOldCert(); } );

	connect( ui->btnMenuGeneral,  &QPushButton::clicked, this, [this](){ changePage(ui->btnMenuGeneral); ui->stackedWidget->setCurrentIndex(0); } );
	connect( ui->btnMenuSigning, &QPushButton::clicked, this, [this](){ changePage(ui->btnMenuSigning); ui->stackedWidget->setCurrentIndex(1); } );
	connect( ui->btnMenuCertificate, &QPushButton::clicked, this, [this](){ changePage(ui->btnMenuCertificate); ui->stackedWidget->setCurrentIndex(2); } );
	connect( ui->btnMenuProxy, &QPushButton::clicked, this, [this](){ changePage(ui->btnMenuProxy); ui->stackedWidget->setCurrentIndex(3); } );
	connect( ui->btnMenuDiagnostics, &QPushButton::clicked, this, [this](){ changePage(ui->btnMenuDiagnostics); ui->stackedWidget->setCurrentIndex(4); } );
	connect( ui->btnMenuInfo, &QPushButton::clicked, this, [this](){ changePage(ui->btnMenuInfo); ui->stackedWidget->setCurrentIndex(5); } );

	connect( this, &SettingsDialog::finished, this, &SettingsDialog::save );
	connect( this, &SettingsDialog::finished, this, [](){ QApplication::restoreOverrideCursor(); } );

	connect( ui->btGeneralChooseDirectory, &QPushButton::clicked, this, &SettingsDialog::openDirectory );
}

void SettingsDialog::checkConnection()
{
	QApplication::setOverrideCursor( Qt::WaitCursor );
	saveProxy();
	CheckConnection connection;
	if(!connection.check("http://ocsp.sk.ee"))
	{
		Application::restoreOverrideCursor();
		FadeInNotification* notification = new FadeInNotification(this, 
			ria::qdigidoc4::colors::MOJO, ria::qdigidoc4::colors::MARZIPAN, 0, 120);
		QString error;
		QString details = connection.errorDetails();
		QTextStream s(&error);
		s << connection.errorString();
		if (!details.isEmpty())
			s << "\n\n" << details << ".";
		notification->start(error, 750, 6000, 1200);
	}
	else
	{
		Application::restoreOverrideCursor();
		FadeInNotification* notification = new FadeInNotification(this, 
			ria::qdigidoc4::colors::WHITE, ria::qdigidoc4::colors::MANTIS, 0, 120);
		notification->start(tr("The connection to certificate status service is successful!"), 750, 3000, 1200);
	}
}

bool SettingsDialog::hasNewerVersion()
{
	QStringList curList = qApp->applicationVersion().split('.');
	QStringList avaList = Configuration::instance().object()["QDIGIDOC4-LATEST"].toString().split('.');
	for( int i = 0; i < std::max<int>(curList.size(), avaList.size()); ++i )
	{
		bool curconv = false, avaconv = false;
		unsigned int cur = curList.value(i).toUInt( &curconv );
		unsigned int ava = avaList.value(i).toUInt( &avaconv );
		if( curconv && avaconv )
		{
			if( cur != ava )
				return cur < ava;
		}
		else
		{
			int status = QString::localeAwareCompare( curList.value(i), avaList.value(i) );
			if( status != 0 )
				return status < 0;
		}
	}
	return false;
}

void SettingsDialog::retranslate(const QString& lang)
{
	emit langChanged(lang);

	qApp->loadTranslation( lang );
	ui->retranslateUi(this);

	QString package;
#ifndef Q_OS_MAC
	QStringList packages = Common::packages({
		"Eesti ID-kaardi tarkvara", "Estonian ID-card software", "estonianidcard", "eID software"});
	if( !packages.isEmpty() )
		package = "<br />" + tr("Base version:") + " " + packages.first();
#endif
	ui->txtNavVersion->setText( tr("%1 version %2, released %3%4")
		.arg( tr("DigiDoc4 client"), qApp->applicationVersion(), BUILD_DATE, package ) );
	updateCert();
	updateDiagnostics();
}

void SettingsDialog::initFunctionality()
{
	selectLanguage();
	connect( ui->rdGeneralEstonian, &QRadioButton::toggled, this, [this](bool checked) { if(checked) retranslate("et"); } );
	connect( ui->rdGeneralEnglish, &QRadioButton::toggled, this, [this](bool checked) { if(checked) retranslate("en"); } );
	connect( ui->rdGeneralRussian, &QRadioButton::toggled, this, [this](bool checked) { if(checked) retranslate("ru"); } );

	updateCert();
#ifdef Q_OS_MAC
	ui->lblDefaultDirectory->hide();
	ui->rdGeneralSameDirectory->hide();
	ui->txtGeneralDirectory->hide();
	ui->btGeneralChooseDirectory->hide();
	ui->rdGeneralSpecifyDirectory->hide();
#else
	ui->txtGeneralDirectory->setText( Settings().value( "Client/DefaultDir" ).toString() );
	if(ui->txtGeneralDirectory->text().isEmpty())
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
#endif

#ifdef Q_OS_WIN
	if (QFile::exists(qApp->applicationDirPath() + "/id-updater.exe"))
	{
		int selected = QProcess::execute("id-updater", {"-status"});
		ui->cmbGeneralCheckUpdatePeriod->setCurrentIndex(selected > 0 && selected < 4 ? selected : 2);
		connect(ui->cmbGeneralCheckUpdatePeriod,
			static_cast<void (QComboBox::*)(int)>(&QComboBox::activated), this, [](int index)
		{
			auto runPrivileged = [](const QString &program, const QString &arguments) {
				ShellExecuteW(nullptr, L"runas", PCWSTR(program.utf16()), PCWSTR(arguments.utf16()), nullptr, SW_HIDE);
			};
			switch(index)
			{
			case 1: return runPrivileged("id-updater", "-daily");
			case 2: return runPrivileged("id-updater", "-weekly");
			case 3: return runPrivileged("id-updater", "-monthly");
			case 4: return runPrivileged("id-updater", "-remove");
			default: break;
			}
		});
	}
	else
	{
		ui->cmbGeneralCheckUpdatePeriod->hide();
		ui->lblGeneralCheckUpdatePeriod->hide();
	}
#endif

	if(Settings(qApp->applicationName()).value( "Client/Type" ).toString() == "asice")
	{
		ui->rdSigningAsice->setChecked(true);
	}
	else
	{
		ui->rdSigningBdoc->setChecked(true);
	}
	connect( ui->rdSigningBdoc, &QRadioButton::toggled, this, [](bool checked) { if(checked) { Settings(qApp->applicationName()).setValueEx( "Client/Type", "bdoc", "bdoc" ); } } );
	connect( ui->rdSigningAsice, &QRadioButton::toggled, this, [](bool checked) { if(checked) { Settings(qApp->applicationName()).setValueEx( "Client/Type", "asice", "bdoc" ); } } );

	ui->chkGeneralTslRefresh->setChecked( qApp->confValue( Application::TSLOnlineDigest ).toBool() );
	connect( ui->chkGeneralTslRefresh, &QCheckBox::toggled, []( bool checked ) { qApp->setConfValue( Application::TSLOnlineDigest, checked ); } );

	ui->tokenRestart->hide();
#ifdef Q_OS_WIN
	connect(ui->tokenRestart, &QPushButton::clicked, this, []{
		qApp->setProperty("restart", true);
		qApp->quit();
	});
	ui->tokenBackend->setChecked(Settings(qApp->applicationName()).value(QStringLiteral("tokenBackend")).toUInt());
	connect(ui->tokenBackend, &QCheckBox::toggled, ui->tokenRestart, [=](bool checked){
		Settings(qApp->applicationName()).setValueEx(QStringLiteral("tokenBackend"), int(checked), 0);
		ui->tokenRestart->show();
	});
#else
	ui->tokenBackend->hide();
#endif


	Settings s;
	s.beginGroup("Client");
	ui->txtSigningRole->setText(s.value("Role").toString());
	ui->txtSigningCity->setText(s.value("City").toString());
	ui->txtSigningCounty->setText(s.value("State").toString());
	ui->txtSigningCountry->setText(s.value("Country").toString());
	ui->txtSigningZipCode->setText(s.value("Zip").toString());

//	d->signOverwrite->setChecked( s.value( "Overwrite", false ).toBool() );

	setProxyEnabled();
	connect( ui->rdProxyNone, &QRadioButton::toggled, this, &SettingsDialog::setProxyEnabled );
	connect( ui->rdProxySystem, &QRadioButton::toggled, this, &SettingsDialog::setProxyEnabled );
	connect( ui->rdProxyManual, &QRadioButton::toggled, this, &SettingsDialog::setProxyEnabled );
	switch(Settings(qApp->applicationName()).value("Client/proxyConfig", 0 ).toInt())
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
	ui->chkIgnoreAccessCert->setChecked( Application::confValue( Application::PKCS12Disable, false ).toBool() );

	ui->chkShowPrintSummary->setChecked(Settings(qApp->applicationName()).value( "Client/ShowPrintSummary", "false" ).toBool());
	connect(ui->chkShowPrintSummary, &QCheckBox::toggled, this, &SettingsDialog::togglePrinting);

	updateDiagnostics();
}

void SettingsDialog::updateCert()
{
	QSslCertificate c = AccessCert::cert();
	if( !c.isNull() )
	{
		ui->txtAccessCert->setText(
			tr("FREE_CERT_EXCEEDED") + "<br /><br />" +
			QString(tr("Issued to: %1<br />Valid to: %2 %3"))
				.arg(CertificateDetails::decodeCN(SslCertificate(c).subjectInfo(QSslCertificate::CommonName)))
				.arg( c.expiryDate().toString("dd.MM.yyyy") )
				.arg( !SslCertificate(c).isValid() ? 
					"<font color='red'>(" + tr("expired") + ")</font>" : "" ) );
	}
	else
	{
		ui->txtAccessCert->setText( 
			tr("FREE_CERT_EXCEEDED") + "<br /><br />" +
			"<b>" + tr("Server access certificate is not installed.") + "</b>" );
	}
	ui->txtAccessCert->adjustSize();
	ui->btnNavShowCertificate->setEnabled( !c.isNull() );
	ui->btnNavShowCertificate->setProperty( "cert", QVariant::fromValue( c ) );
}

void SettingsDialog::selectLanguage()
{
	if(Settings::language() == "en")
		ui->rdGeneralEnglish->setChecked(true);
	else if(Settings::language() == "ru")
		ui->rdGeneralRussian->setChecked(true);
	else
		ui->rdGeneralEstonian->setChecked(true);
}

void SettingsDialog::setProxyEnabled()
{
	ui->txtProxyHost->setEnabled(ui->rdProxyManual->isChecked());
	ui->txtProxyPort->setEnabled(ui->rdProxyManual->isChecked());
	ui->txtProxyUsername->setEnabled(ui->rdProxyManual->isChecked());
	ui->txtProxyPassword->setEnabled(ui->rdProxyManual->isChecked());
	ui->chkProxyEnableForSSL->setEnabled(ui->rdProxyManual->isChecked());
}

void SettingsDialog::updateProxy()
{
	ui->txtProxyHost->setText(Application::confValue( Application::ProxyHost ).toString());
	ui->txtProxyPort->setText(Application::confValue( Application::ProxyPort ).toString());
	ui->txtProxyUsername->setText(Application::confValue( Application::ProxyUser ).toString());
	ui->txtProxyPassword->setText(Application::confValue( Application::ProxyPass ).toString());
	ui->chkProxyEnableForSSL->setChecked(Application::confValue( Application::ProxySSL ).toBool());
}

void SettingsDialog::save()
{
#ifndef Q_OS_MAC
	Settings().setValue("Client/DefaultDir", ui->txtGeneralDirectory->text());
#endif

	Application::setConfValue( Application::PKCS12Disable, ui->chkIgnoreAccessCert->isChecked() );
	saveProxy();
	saveSignatureInfo(
		ui->txtSigningRole->text(),
		ui->txtSigningCity->text(),
		ui->txtSigningCounty->text(),
		ui->txtSigningCountry->text(),
		ui->txtSigningZipCode->text()
		);

	Settings(qApp->applicationName()).setValue("Client/ShowPrintSummary", ui->chkShowPrintSummary->isChecked() );
}

void SettingsDialog::saveProxy()
{
	if(ui->rdProxyNone->isChecked())
	{
		Settings(qApp->applicationName()).setValue("Client/proxyConfig", 0);
	}
	else if(ui->rdProxySystem->isChecked())
	{
		Settings(qApp->applicationName()).setValue("Client/proxyConfig", 1);
	}
	else if(ui->rdProxyManual->isChecked())
	{
		Settings(qApp->applicationName()).setValue("Client/proxyConfig", 2);
	}

	Application::setConfValue( Application::ProxyHost, ui->txtProxyHost->text() );
	Application::setConfValue( Application::ProxyPort, ui->txtProxyPort->text() );
	Application::setConfValue( Application::ProxyUser, ui->txtProxyUsername->text() );
	Application::setConfValue( Application::ProxyPass, ui->txtProxyPassword->text() );
	Application::setConfValue( Application::ProxySSL, ui->chkProxyEnableForSSL->isChecked() );
	loadProxy(digidoc::Conf::instance());
	updateProxy();
}

void SettingsDialog::loadProxy( const digidoc::Conf *conf )
{
	switch(Settings(qApp->applicationName()).value("Client/proxyConfig", 0).toUInt())
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
		Settings().setValue( "Client/DefaultDir", dir );
		ui->txtGeneralDirectory->setText( dir );
	}
}


void SettingsDialog::saveSignatureInfo(
		const QString &role,
		const QString &city,
		const QString &state,
		const QString &country,
		const QString &zip)
{
	Settings s;
	s.beginGroup("Client");
	s.setValue("Role", role);
	s.setValue("City", city);
	s.setValue("State", state),
	s.setValue("Country", country);
	s.setValue("Zip", zip);
}

void SettingsDialog::updateDiagnostics()
{
	ui->txtDiagnostics->setEnabled(false);

	QApplication::setOverrideCursor( Qt::WaitCursor );
	Diagnostics *worker = new Diagnostics();
	connect(worker, &Diagnostics::update, ui->txtDiagnostics, &QTextBrowser::insertHtml, Qt::QueuedConnection);
	connect(worker, &Diagnostics::destroyed, this, [=]{
		if(!appletVersion.isEmpty())
		{
			QString info;
			QTextStream s(&info);
			s << "<b>" << tr("Applet") << ":</b> " << appletVersion;
			ui->txtDiagnostics->append(info);
		}
		ui->txtDiagnostics->setEnabled(true);
		ui->txtDiagnostics->moveCursor(QTextCursor::Start) ;
		ui->txtDiagnostics->ensureCursorVisible() ;
		QApplication::restoreOverrideCursor();
	});
	QThreadPool::globalInstance()->start( worker );
	if(Settings(QSettings::SystemScope).value("disableSave", false).toBool())
	{
		ui->btnNavSaveReport->hide();
	}
}

void SettingsDialog::installCert()
{
	QString certpath = "";

	QFile file( !certpath.isEmpty() ? certpath :
		FileDialog::getOpenFileName( this, tr("Select server access certificate"),
		QString(), tr("Server access certificates (*.p12 *.p12d *.pfx)") ) );
	if(!file.exists())
		return;
	QString pass = QInputDialog::getText( this, tr("Password"),
		tr("Enter server access certificate password."), QLineEdit::Password );
	if(pass.isEmpty())
		return;

	if(!file.open(QFile::ReadOnly))
		return;

	PKCS12Certificate p12(&file, pass);
	switch(p12.error())
	{
	case PKCS12Certificate::NullError: break;
	case PKCS12Certificate::InvalidPasswordError:
		QMessageBox::warning(this, tr("Select server access certificate"), tr("Invalid password"));
		return;
	default:
		QMessageBox::warning(this, tr("Select server access certificate"),
			tr("Server access certificate error: %1").arg(p12.errorString()));
		return;
	}

#ifndef Q_OS_MAC
	if( file.fileName() == QDir::fromNativeSeparators( Application::confValue( Application::PKCS12Cert ).toString() ) )
		return;
#endif
	file.reset();
	AccessCert().installCert( file.readAll(), pass );
	updateCert();
}

void SettingsDialog::removeCert()
{
	AccessCert().remove();
	updateCert();
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

	ui->btnNavUseByDefault->setVisible(button == ui->btnMenuCertificate);
	ui->btnNavInstallManually->setVisible(button == ui->btnMenuCertificate);
	ui->btnNavShowCertificate->setVisible(button == ui->btnMenuCertificate);
	ui->btAppStore->setVisible(button == ui->btnMenuGeneral && hasNewerVersion());
	ui->btnFirstRun->setVisible(button == ui->btnMenuGeneral);
	ui->btnCheckConnection->setVisible(button == ui->btnMenuProxy);
	ui->btnNavSaveReport->setVisible(button == ui->btnMenuDiagnostics);
#ifdef Q_OS_WIN
	ui->btnNavFromHistory->setVisible(button == ui->btnMenuGeneral);
#else
	ui->btnNavFromHistory->setVisible(false);
#endif
}

int SettingsDialog::exec()
{
	Overlay overlay( parentWidget() );
	overlay.show();
	auto rc = QDialog::exec();
	overlay.close();

	return rc;
}

void SettingsDialog::saveDiagnostics()
{
	QString filename = FileDialog::getSaveFileName( this, tr("Save as"), QString( "%1/%2_diagnostics.txt")
		.arg( QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation), qApp->applicationName() ),
		tr("Text files (*.txt)") );
	if( filename.isEmpty() )
		return;
	QFile f( filename );
	if( !f.open( QIODevice::WriteOnly ) || !f.write( ui->txtDiagnostics->toPlainText().toUtf8() ) )
		QMessageBox::warning( this, tr("Error occurred"), tr("Failed write to file!") );
}
