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
#ifdef Q_OS_WIN
#include "CertStore.h"
#endif
#include "CheckConnection.h"
#include "Colors.h"
#include "FileDialog.h"
#include "QSigner.h"
#include "QSmartCard.h"
#include "Styles.h"
#include "SslCertificate.h"
#include "Diagnostics.h"
#include "dialogs/CertificateDetails.h"
#include "dialogs/FirstRun.h"
#include "effects/ButtonHoverFilter.h"
#include "effects/Overlay.h"
#include "effects/FadeInNotification.h"

#include "common/Configuration.h"

#include <digidocpp/Conf.h>

#include <QtCore/QFile>
#include <QtCore/QIODevice>
#include <QtCore/QJsonObject>
#include <QtCore/QStandardPaths>
#include <QtCore/QSysInfo>
#include <QtCore/QThread>
#include <QtCore/QThreadPool>
#include <QtCore/QUrl>
#include <QtGui/QDesktopServices>
#include <QtCore/QSettings>
#include <QtNetwork/QNetworkProxy>
#include <QtNetwork/QSslCertificate>
#include <QtWidgets/QInputDialog>
#include <QtWidgets/QMessageBox>
#include <QtWidgets/QTextBrowser>

SettingsDialog::SettingsDialog(QWidget *parent)
	: QDialog(parent)
	, ui(new Ui::SettingsDialog)
{
	Overlay *overlay = new Overlay(parent->topLevelWidget());
	overlay->show();
	connect(this, &SettingsDialog::destroyed, overlay, &Overlay::deleteLater);

	ui->setupUi(this);
#ifdef Q_OS_MAC
	setWindowFlags(Qt::Popup);
	move(parent->geometry().center() - geometry().center());
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
	ui->btnMenuCertificate->setFont(condensed12);
	ui->btnMenuProxy->setFont(condensed12);
	ui->btnMenuDiagnostics->setFont(condensed12);
	ui->btnMenuInfo->setFont(condensed12);

	// pageGeneral
	ui->lblGeneralLang->setFont(headerFont);
	ui->lblDefaultDirectory->setFont(headerFont);

	ui->rdGeneralEstonian->setFont(regularFont);
	ui->rdGeneralRussian->setFont(regularFont);
	ui->rdGeneralEnglish->setFont(regularFont);

	ui->rdGeneralSameDirectory->setFont(regularFont);
	ui->rdGeneralSpecifyDirectory->setFont(regularFont);
	ui->btGeneralChooseDirectory->setFont(regularFont);
	ui->txtGeneralDirectory->setFont(regularFont);

	ui->chkGeneralTslRefresh->setFont(regularFont);
	ui->tokenBackend->setFont(regularFont);

	ui->chkShowPrintSummary->setFont(regularFont);
	ui->chkRoleAddressInfo->setFont(regularFont);

	// pageServices
	ui->lblRevocation->setFont(headerFont);
	ui->lblAccessCert->setFont(regularFont);
	ui->txtAccessCert->setFont(regularFont);
	ui->btInstallManually->setFont(condensed12);
	ui->btShowCertificate->setFont(condensed12);
	ui->chkIgnoreAccessCert->setFont(regularFont);
	ui->lblTimeStamp->setFont(headerFont);
	ui->rdTimeStamp->setFont(regularFont);
	ui->lblMID->setFont(headerFont);
	ui->rdMIDUUID->setFont(regularFont);
	ui->helpRevocation->installEventFilter(new ButtonHoverFilter(QStringLiteral(":/images/icon_Abi.svg"), QStringLiteral(":/images/icon_Abi_hover.svg"), this));
	ui->helpTimeStamp->installEventFilter(new ButtonHoverFilter(QStringLiteral(":/images/icon_Abi.svg"), QStringLiteral(":/images/icon_Abi_hover.svg"), this));
	ui->helpMID->installEventFilter(new ButtonHoverFilter(QStringLiteral(":/images/icon_Abi.svg"), QStringLiteral(":/images/icon_Abi_hover.svg"), this));

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
	ui->structureFunds->load(QStringLiteral(":/images/Struktuurifondid.svg"));
	ui->pageInfoLayout->setAlignment(ui->structureFunds, Qt::AlignCenter);
	ui->contact->setFont(regularFont);
	ui->txtDiagnostics->setFont(regularFont);

	// pageInfo
	ui->txtInfo->setFont(regularFont);

	// navigationArea
	ui->txtNavVersion->setFont(Styles::font( Styles::Regular, 12 ));
	ui->btnNavFromHistory->setFont(condensed12);

	ui->btnNavUseByDefault->setFont(condensed12);
	ui->btnFirstRun->setFont(condensed12);
	ui->btnRefreshConfig->setFont(condensed12);
	ui->btnNavSaveReport->setFont(condensed12);
	ui->btnCheckConnection->setFont(condensed12);

	ui->btNavClose->setFont(Styles::font( Styles::Condensed, 14 ));

	changePage(ui->btnMenuGeneral);

#ifdef CONFIG_URL
	connect(&Configuration::instance(), &Configuration::finished, this, [=](bool /*update*/, const QString &error){
		if(error.isEmpty())
		{
			WarningDialog dlg(tr("Digidoc4 client configuration update was successful."), qApp->activeWindow());
			dlg.exec();
			return;
		}

		WarningDialog dlg(tr("Checking updates has failed.") + "<br />" + tr("Please try again."), error, qApp->activeWindow());
		dlg.exec();
	});
#endif

	connect( ui->btNavClose, &QPushButton::clicked, this, &SettingsDialog::accept );
	connect( this, &SettingsDialog::finished, this, &SettingsDialog::close );

	connect(ui->btnCheckConnection, &QPushButton::clicked, this, &SettingsDialog::checkConnection);
	connect(ui->btnFirstRun, &QPushButton::clicked, this, [this] {
		FirstRun dlg(this);
		connect(&dlg, &FirstRun::langChanged, this, [this](const QString &lang) {
			retranslate(lang);
			selectLanguage();
		});
		dlg.exec();
	});
	connect(ui->btnRefreshConfig, &QPushButton::clicked, this, [] {
#ifdef CONFIG_URL
		Configuration::instance().update(true);
#endif
		QString cache = qApp->confValue(Application::TSLCache).toString();
		const QStringList tsllist = QDir(QStringLiteral(":/TSL/")).entryList();
		for(const QString &file: tsllist)
		{
			const QString target = cache + "/" + file;
			if(!QFile::exists(target) ||
				Application::readTSLVersion(":/TSL/" + file) > Application::readTSLVersion(target))
			{
				const QStringList cleanup = QDir(cache, file + QStringLiteral("*")).entryList();
				for(const QString &rm: cleanup)
					QFile::remove(cache + "/" + rm);
				QFile::copy(":/TSL/" + file, target);
				QFile::setPermissions(target, QFile::Permissions(0x6444));
			}
		
		}
	});
	connect( ui->btnNavUseByDefault, &QPushButton::clicked, this, &SettingsDialog::useDefaultSettings );
	connect( ui->btnNavSaveReport, &QPushButton::clicked, this, &SettingsDialog::saveDiagnostics );
#ifdef Q_OS_WIN
	connect(ui->btnNavFromHistory, &QPushButton::clicked, this, [] {
		// remove certificates (having %ESTEID% text) from browsing history of Internet Explorer and/or Google Chrome, and do it for all users.
		QSmartCardData data = qApp->signer()->smartcard()->data();
		CertStore s;
		for(const QSslCertificate &c: s.list())
		{
			if(c == data.authCert() || c == data.signCert())
				continue;
			if(c.subjectInfo(QSslCertificate::Organization).join(QString()).contains(QStringLiteral("ESTEID"), Qt::CaseInsensitive) ||
				c.issuerInfo(QSslCertificate::Organization).contains(QStringLiteral("SK ID Solutions AS"), Qt::CaseInsensitive))
				s.remove( c );
		}
		qApp->showWarning( tr("Redundant certificates have been successfully removed.") );
	});
#endif

	ui->pageGroup->setId(ui->btnMenuGeneral, GeneralSettings);
	ui->pageGroup->setId(ui->btnMenuCertificate, AccessCertSettings);
	ui->pageGroup->setId(ui->btnMenuProxy, NetworkSettings);
	ui->pageGroup->setId(ui->btnMenuDiagnostics, DiagnosticsSettings);
	ui->pageGroup->setId(ui->btnMenuInfo, LicenseSettings);
	connect(ui->pageGroup, static_cast<void (QButtonGroup::*)(QAbstractButton*)>(&QButtonGroup::buttonClicked), this, &SettingsDialog::changePage);

	initFunctionality();
	updateVersion();
	updateDiagnostics();
}

SettingsDialog::SettingsDialog(int page, QWidget *parent)
	: SettingsDialog(parent)
{
	changePage(ui->pageGroup->button(page));
}


SettingsDialog::~SettingsDialog()
{
	QApplication::restoreOverrideCursor();
	delete ui;
}

void SettingsDialog::checkConnection()
{
	QApplication::setOverrideCursor( Qt::WaitCursor );
	saveProxy();
	CheckConnection connection;
	if(!connection.check(QStringLiteral("https://id.eesti.ee/config.json")))
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

void SettingsDialog::retranslate(const QString& lang)
{
	emit langChanged(lang);

	qApp->loadTranslation( lang );
	ui->retranslateUi(this);
	updateVersion();
	updateCert();
	updateDiagnostics();
}

void SettingsDialog::initFunctionality()
{
	// pageGeneral
	selectLanguage();
	connect( ui->rdGeneralEstonian, &QRadioButton::toggled, this, [this](bool checked) { if(checked) retranslate(QStringLiteral("et")); } );
	connect( ui->rdGeneralEnglish, &QRadioButton::toggled, this, [this](bool checked) { if(checked) retranslate(QStringLiteral("en")); } );
	connect( ui->rdGeneralRussian, &QRadioButton::toggled, this, [this](bool checked) { if(checked) retranslate(QStringLiteral("ru")); } );

	ui->chkGeneralTslRefresh->setChecked(qApp->confValue(Application::TSLOnlineDigest).toBool());
	connect(ui->chkGeneralTslRefresh, &QCheckBox::toggled, [](bool checked) {
		qApp->setConfValue(Application::TSLOnlineDigest, checked);
	});
	ui->chkShowPrintSummary->setChecked(QSettings().value(QStringLiteral("ShowPrintSummary"), false).toBool());
	connect(ui->chkShowPrintSummary, &QCheckBox::toggled, this, &SettingsDialog::togglePrinting);
	connect(ui->chkShowPrintSummary, &QCheckBox::toggled, this, [](bool checked) {
		Common::setValueEx(QStringLiteral("ShowPrintSummary"), checked, false);
	});
	ui->chkRoleAddressInfo->setChecked(QSettings().value(QStringLiteral("RoleAddressInfo"), false).toBool());
	connect(ui->chkRoleAddressInfo, &QCheckBox::toggled, this, [](bool checked) {
		Common::setValueEx(QStringLiteral("RoleAddressInfo"), checked, false);
	});

#ifdef Q_OS_MAC
	ui->lblDefaultDirectory->hide();
	ui->rdGeneralSameDirectory->hide();
	ui->txtGeneralDirectory->hide();
	ui->btGeneralChooseDirectory->hide();
	ui->rdGeneralSpecifyDirectory->hide();
#else
	connect(ui->btGeneralChooseDirectory, &QPushButton::clicked, this, [=]{
		QString dir = FileDialog::getExistingDirectory(this, tr("Select folder"),
			QSettings().value(QStringLiteral("DefaultDir")).toString());
		if(!dir.isEmpty())
		{
			ui->rdGeneralSpecifyDirectory->setChecked(true);
			Common::setValueEx(QStringLiteral("DefaultDir"), dir, QString());
			ui->txtGeneralDirectory->setText(dir);
		}
	});
	connect(ui->rdGeneralSpecifyDirectory, &QRadioButton::toggled, this, [=](bool enable) {
		ui->btGeneralChooseDirectory->setVisible(enable);
		ui->txtGeneralDirectory->setVisible(enable);
		if(!enable)
			ui->txtGeneralDirectory->clear();
	});
	ui->txtGeneralDirectory->setText(QSettings().value(QStringLiteral("DefaultDir")).toString());
	if(ui->txtGeneralDirectory->text().isEmpty())
		ui->rdGeneralSameDirectory->setChecked(true);
	connect(ui->txtGeneralDirectory, &QLineEdit::textChanged, this, [](const QString &text) {
		Common::setValueEx(QStringLiteral("DefaultDir"), text, QString());
	});
#endif

	ui->tokenRestart->hide();
#ifdef Q_OS_WIN
	connect(ui->tokenRestart, &QPushButton::clicked, this, []{
		qApp->setProperty("restart", true);
		qApp->quit();
	});
	ui->tokenBackend->setChecked(QSettings().value(QStringLiteral("tokenBackend")).toUInt());
	connect(ui->tokenBackend, &QCheckBox::toggled, ui->tokenRestart, [=](bool checked) {
		Common::setValueEx(QStringLiteral("tokenBackend"), int(checked), 0);
		ui->tokenRestart->show();
	});
#else
	ui->tokenBackend->hide();
#endif

	// pageProxy
	connect(this, &SettingsDialog::finished, this, &SettingsDialog::saveProxy);
	setProxyEnabled();
	connect( ui->rdProxyNone, &QRadioButton::toggled, this, &SettingsDialog::setProxyEnabled );
	connect( ui->rdProxySystem, &QRadioButton::toggled, this, &SettingsDialog::setProxyEnabled );
	connect( ui->rdProxyManual, &QRadioButton::toggled, this, &SettingsDialog::setProxyEnabled );
	switch(QSettings().value(QStringLiteral("ProxyConfig"), 0).toInt())
	{
	case 1: ui->rdProxySystem->setChecked(true); break;
	case 2: ui->rdProxyManual->setChecked(true); break;
	default: ui->rdProxyNone->setChecked(true); break;
	}
	updateProxy();

	// pageServices
	updateCert();
	connect(ui->btShowCertificate, &QPushButton::clicked, this, [this] {
		CertificateDetails::showCertificate(SslCertificate(AccessCert::cert()), this);
	});
	connect(ui->btInstallManually, &QPushButton::clicked, this, &SettingsDialog::installCert);
	ui->chkIgnoreAccessCert->setChecked(Application::confValue(Application::PKCS12Disable, false).toBool());
	connect(ui->chkIgnoreAccessCert, &QCheckBox::toggled, this, [](bool checked) {
		Application::setConfValue(Application::PKCS12Disable, checked);
	});
#ifdef CONFIG_URL
	ui->rdTimeStamp->setPlaceholderText(Configuration::instance().object().value(QStringLiteral("TSA-URL")).toString());
#endif
	QString TSA_URL = qApp->confValue(Application::TSAUrl).toString();
	ui->rdTimeStamp->setText(ui->rdTimeStamp->placeholderText() == TSA_URL ? QString() : TSA_URL);
	connect(ui->rdTimeStamp, &QLineEdit::textChanged, this, [](const QString &url) {
		qApp->setConfValue(Application::TSAUrl, url);
	});
	ui->rdMIDUUID->setText(QSettings().value(QStringLiteral("MIDUUID")).toString());
	connect(ui->rdMIDUUID, &QLineEdit::textChanged, this, [](const QString &text) {
		Common::setValueEx(QStringLiteral("MIDUUID"), text, QString());
		Common::setValueEx(QStringLiteral("SIDUUID"), text, QString());
	});
	connect(ui->helpRevocation, &QToolButton::clicked, this, []{
		QDesktopServices::openUrl(tr("https://www.id.ee/index.php?id=39245"));
	});
	connect(ui->helpTimeStamp, &QToolButton::clicked, this, []{
		QDesktopServices::openUrl(tr("https://www.id.ee/index.php?id=39076"));
	});
	connect(ui->helpMID, &QToolButton::clicked, this, []{
		QDesktopServices::openUrl(tr("https://www.id.ee/index.php?id=39023"));
	});
}

void SettingsDialog::updateCert()
{
	QSslCertificate c = AccessCert::cert();
	if( !c.isNull() )
	{
		ui->txtAccessCert->setText(
			tr("Issued to: %1<br />Valid to: %2 %3").arg(
				CertificateDetails::decodeCN(SslCertificate(c).subjectInfo(QSslCertificate::CommonName)),
				c.expiryDate().toString(QStringLiteral("dd.MM.yyyy")),
				!SslCertificate(c).isValid() ? "<font color='red'>(" + tr("expired") + ")</font>" : QString()));
	}
	else
	{
		ui->txtAccessCert->setText( 
			"<b>" + tr("Server access certificate is not installed.") + "</b>" );
	}
	ui->btShowCertificate->setEnabled(!c.isNull());
	ui->btShowCertificate->setProperty("cert", QVariant::fromValue(c));
}

void SettingsDialog::selectLanguage()
{
	if(Common::language() == QStringLiteral("en"))
		ui->rdGeneralEnglish->setChecked(true);
	else if(Common::language() == QStringLiteral("ru"))
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

void SettingsDialog::updateVersion()
{
	ui->txtNavVersion->setText(tr("%1 version %2, released %3")
		.arg(tr("DigiDoc4 client"), qApp->applicationVersion(), QStringLiteral(BUILD_DATE)));
}

void SettingsDialog::saveProxy()
{
	if(ui->rdProxyNone->isChecked())
		Common::setValueEx(QStringLiteral("ProxyConfig"), 0, 0);
	else if(ui->rdProxySystem->isChecked())
		Common::setValueEx(QStringLiteral("ProxyConfig"), 1, 0);
	else if(ui->rdProxyManual->isChecked())
		Common::setValueEx(QStringLiteral("ProxyConfig"), 2, 0);
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
	switch(QSettings().value(QStringLiteral("ProxyConfig"), 0).toUInt())
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
			QString::fromStdString(conf->proxyPort()).toUShort(),
			QString::fromStdString(conf->proxyUser()),
			QString::fromStdString(conf->proxyPass())));
		break;
	}
}

void SettingsDialog::updateDiagnostics()
{
	ui->txtDiagnostics->setEnabled(false);
	ui->txtDiagnostics->clear();

	QApplication::setOverrideCursor( Qt::WaitCursor );
	Diagnostics *worker = new Diagnostics();
	connect(worker, &Diagnostics::update, ui->txtDiagnostics, &QTextBrowser::insertHtml, Qt::QueuedConnection);
	connect(worker, &Diagnostics::destroyed, this, [=]{
		QSmartCardData t = qApp->signer()->smartcard()->data();
		QString appletVersion = t.isNull() ? QString() : t.appletVersion();
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
	if(QSettings(QSettings::SystemScope, nullptr).value(QStringLiteral("disableSave"), false).toBool())
	{
		ui->btnNavSaveReport->hide();
	}
}

void SettingsDialog::installCert()
{
	QFile file(FileDialog::getOpenFileName(this, tr("Select server access certificate"),
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

void SettingsDialog::useDefaultSettings()
{
	AccessCert().remove();
	updateCert();
	ui->rdTimeStamp->clear();
	ui->rdMIDUUID->clear();
}

void SettingsDialog::changePage(QAbstractButton *button)
{
	button->setChecked(true);
	ui->stackedWidget->setCurrentIndex(ui->pageGroup->id(button));
	ui->btnNavUseByDefault->setVisible(button == ui->btnMenuCertificate);
	ui->btnFirstRun->setVisible(button == ui->btnMenuGeneral);
	ui->btnRefreshConfig->setVisible(button == ui->btnMenuGeneral);
	ui->btnCheckConnection->setVisible(button == ui->btnMenuProxy);
	ui->btnNavSaveReport->setVisible(button == ui->btnMenuDiagnostics);
#ifdef Q_OS_WIN
	ui->btnNavFromHistory->setVisible(button == ui->btnMenuGeneral);
#else
	ui->btnNavFromHistory->setVisible(false);
#endif
}

void SettingsDialog::saveDiagnostics()
{
	QString filename = FileDialog::getSaveFileName(this, tr("Save as"), QStringLiteral( "%1/%2_diagnostics.txt")
		.arg( QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation), qApp->applicationName() ),
		tr("Text files (*.txt)") );
	if( filename.isEmpty() )
		return;
	QFile f( filename );
	if(!f.open(QIODevice::WriteOnly|QIODevice::Text) || !f.write(ui->txtDiagnostics->toPlainText().toUtf8()))
		QMessageBox::warning( this, tr("Error occurred"), tr("Failed write to file!") );
}
