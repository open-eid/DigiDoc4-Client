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
#include "Diagnostics.h"
#include "FileDialog.h"
#include "QSigner.h"
#include "QSmartCard.h"
#include "Styles.h"
#include "SslCertificate.h"
#include "TokenData.h"
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
#include <QtCore/QProcess>
#include <QtCore/QSettings>
#include <QtCore/QStandardPaths>
#include <QtCore/QThread>
#include <QtCore/QThreadPool>
#include <QtCore/QUrl>
#include <QtGui/QDesktopServices>
#include <QtNetwork/QNetworkProxy>
#include <QtWidgets/QInputDialog>
#include <QtWidgets/QTextBrowser>

#include <algorithm>

SettingsDialog::SettingsDialog(QWidget *parent)
	: QDialog(parent)
	, ui(new Ui::SettingsDialog)
{
	new Overlay(this, parent->topLevelWidget());

	ui->setupUi(this);
	setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint);
	move(parent->geometry().center() - geometry().center());

	QFont headerFont = Styles::font(Styles::Regular, 18, QFont::Bold);
	QFont regularFont = Styles::font(Styles::Regular, 14);
	QFont condensed12 = Styles::font(Styles::Condensed, 12);
	headerFont.setPixelSize(18);
	regularFont.setPixelSize(14);
	condensed12.setPixelSize(12);

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
	ui->chkLibdigidocppDebug->setFont(regularFont);

	// pageServices
	ui->lblRevocation->setFont(headerFont);
	ui->lblAccessCert->setFont(regularFont);
	ui->txtAccessCert->setFont(regularFont);
	ui->btInstallManually->setFont(condensed12);
	ui->btShowCertificate->setFont(condensed12);
	ui->chkIgnoreAccessCert->setFont(regularFont);
	ui->lblTimeStamp->setFont(headerFont);
	ui->rdTimeStampDefault->setFont(regularFont);
	ui->rdTimeStampCustom->setFont(regularFont);
	ui->txtTimeStamp->setFont(regularFont);
	ui->lblMID->setFont(headerFont);
	ui->rdMIDUUIDDefault->setFont(regularFont);
	ui->rdMIDUUIDCustom->setFont(regularFont);
	ui->txtMIDUUID->setFont(regularFont);
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
	ui->btnNavSaveLibdigidocpp->setFont(condensed12);
	ui->btnCheckConnection->setFont(condensed12);

	ui->btNavClose->setFont(Styles::font( Styles::Condensed, 14 ));

	changePage(ui->btnMenuGeneral);

#ifdef CONFIG_URL
	connect(&Configuration::instance(), &Configuration::finished, this, [=](bool /*update*/, const QString &error){
		if(!error.isEmpty()) {
			WarningDialog(tr("Checking updates has failed.") + "<br />" + tr("Please try again."), error, this).exec();
			return;
		}
		WarningDialog(tr("DigiDoc4 client configuration update was successful."), this).exec();
#ifdef Q_OS_WIN
		QString path = qApp->applicationDirPath() + QStringLiteral("/id-updater.exe");
		if (QFile::exists(path))
			QProcess::startDetached(path, {});
#endif
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
	connect( ui->btnNavSaveReport, &QPushButton::clicked, this, [=]{
		saveFile(QStringLiteral("diagnostics.txt"), ui->txtDiagnostics->toPlainText().toUtf8());
	});
	connect(ui->btnNavSaveLibdigidocpp, &QPushButton::clicked, this, [=]{
		QFile f(QStringLiteral("%1/libdigidocpp.log").arg(QDir::tempPath()));
		if(f.open(QFile::ReadOnly|QFile::Text))
			saveFile(QStringLiteral("libdigidocpp.txt"), f.readAll());
		f.remove();
		ui->btnNavSaveLibdigidocpp->hide();
	});
#ifdef Q_OS_WIN
	connect(ui->btnNavFromHistory, &QPushButton::clicked, this, [] {
		// remove certificates (having %ESTEID% text) from browsing history of Internet Explorer and/or Google Chrome, and do it for all users.
		QList<TokenData> cache = qApp->signer()->cache();
		CertStore s;
		for(const QSslCertificate &c: s.list())
		{
			if(std::any_of(cache.cbegin(), cache.cend(), [&](const TokenData &token) { return token.cert() == c; }))
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
	connect(ui->pageGroup, QOverload<QAbstractButton*>::of(&QButtonGroup::buttonClicked), this, &SettingsDialog::changePage);

	initFunctionality();
	updateVersion();
	updateDiagnostics();
}

SettingsDialog::SettingsDialog(int page, QWidget *parent)
	: SettingsDialog(parent)
{
	showPage(page);
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
	QSettings s;

	// pageGeneral
	selectLanguage();
	connect( ui->rdGeneralEstonian, &QRadioButton::toggled, this, [this](bool checked) { if(checked) retranslate(QStringLiteral("et")); } );
	connect( ui->rdGeneralEnglish, &QRadioButton::toggled, this, [this](bool checked) { if(checked) retranslate(QStringLiteral("en")); } );
	connect( ui->rdGeneralRussian, &QRadioButton::toggled, this, [this](bool checked) { if(checked) retranslate(QStringLiteral("ru")); } );

	ui->chkGeneralTslRefresh->setChecked(qApp->confValue(Application::TSLOnlineDigest).toBool());
	connect(ui->chkGeneralTslRefresh, &QCheckBox::toggled, [](bool checked) {
		qApp->setConfValue(Application::TSLOnlineDigest, checked);
	});
	ui->chkShowPrintSummary->setChecked(s.value(QStringLiteral("ShowPrintSummary"), false).toBool());
	connect(ui->chkShowPrintSummary, &QCheckBox::toggled, this, &SettingsDialog::togglePrinting);
	connect(ui->chkShowPrintSummary, &QCheckBox::toggled, this, [](bool checked) {
		setValueEx(QStringLiteral("ShowPrintSummary"), checked, false);
	});
	ui->chkRoleAddressInfo->setChecked(s.value(QStringLiteral("RoleAddressInfo"), false).toBool());
	connect(ui->chkRoleAddressInfo, &QCheckBox::toggled, this, [](bool checked) {
		setValueEx(QStringLiteral("RoleAddressInfo"), checked, false);
	});
	ui->chkLibdigidocppDebug->setChecked(s.value(QStringLiteral("LibdigidocppDebug"), false).toBool());
	connect(ui->chkLibdigidocppDebug, &QCheckBox::toggled, this, [](bool checked) {
		setValueEx(QStringLiteral("LibdigidocppDebug"), checked, false);
		if(!checked)
			return;
#ifdef Q_OS_MAC
		WarningDialog(tr("Restart DigiDoc4 Client to activate logging. Read more "
			"<a href=\"https://www.id.ee/en/article/log-file-generation-in-digidoc4-client/\">here</a>.")).exec();
#else
		WarningDialog dlg(tr("Restart DigiDoc4 Client to activate logging. Read more "
			"<a href=\"https://www.id.ee/en/article/log-file-generation-in-digidoc4-client/\">here</a>. Restart now?"), qApp->activeWindow());
		dlg.setCancelText(tr("NO"));
		dlg.addButton(tr("YES"), 1) ;
		if(dlg.exec() == 1) {
			qApp->setProperty("restart", true);
			qApp->quit();
		}
#endif
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
			setValueEx(QStringLiteral("DefaultDir"), dir);
			ui->txtGeneralDirectory->setText(dir);
		}
	});
	connect(ui->rdGeneralSpecifyDirectory, &QRadioButton::toggled, this, [=](bool enable) {
		ui->btGeneralChooseDirectory->setVisible(enable);
		ui->txtGeneralDirectory->setVisible(enable);
		if(!enable)
			ui->txtGeneralDirectory->clear();
	});
	ui->txtGeneralDirectory->setText(s.value(QStringLiteral("DefaultDir")).toString());
	if(ui->txtGeneralDirectory->text().isEmpty())
		ui->rdGeneralSameDirectory->setChecked(true);
	connect(ui->txtGeneralDirectory, &QLineEdit::textChanged, this, [](const QString &text) {
		setValueEx(QStringLiteral("DefaultDir"), text);
	});
#endif

#ifdef Q_OS_WIN
	ui->tokenBackend->setChecked(s.value(QStringLiteral("tokenBackend")).toUInt());
	connect(ui->tokenBackend, &QCheckBox::toggled, this, [=](bool checked) {
		setValueEx(QStringLiteral("tokenBackend"), int(checked), 0);
		if(qApp->signer()->apiType() == QSigner::ApiType(checked))
			return;

		WarningDialog dlg(tr("Applying this setting requires application restart. Restart now?"), this);
		dlg.setCancelText(tr("NO"));
		dlg.addButton(tr("YES"), 1) ;
		if(dlg.exec() == 1) {
			qApp->setProperty("restart", true);
			qApp->quit();
		}
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
	switch(s.value(QStringLiteral("ProxyConfig"), 0).toInt())
	{
	case 1: ui->rdProxySystem->setChecked(true); break;
	case 2: ui->rdProxyManual->setChecked(true); break;
	default: ui->rdProxyNone->setChecked(true); break;
	}

	ui->chkProxyEnableForSSL->setDisabled((s.value(QStringLiteral("ProxyConfig"), 0).toInt() != 2));
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

	connect(ui->rdTimeStampCustom, &QRadioButton::toggled, ui->txtTimeStamp, [=](bool checked) {
		ui->txtTimeStamp->setEnabled(checked);
		setValueEx(QStringLiteral("TSA-URL-CUSTOM"), checked, QSettings().contains(QStringLiteral("TSA-URL")));
	});
	ui->rdTimeStampCustom->setChecked(s.value(QStringLiteral("TSA-URL-CUSTOM"), s.contains(QStringLiteral("TSA-URL"))).toBool());
#ifdef CONFIG_URL
	ui->txtTimeStamp->setPlaceholderText(Configuration::instance().object().value(QStringLiteral("TSA-URL")).toString());
#endif
	QString TSA_URL = s.value(QStringLiteral("TSA-URL"), qApp->confValue(Application::TSAUrl)).toString();
	ui->txtTimeStamp->setText(ui->txtTimeStamp->placeholderText() == TSA_URL ? QString() : TSA_URL);
	connect(ui->txtTimeStamp, &QLineEdit::textChanged, this, [](const QString &url) {
		qApp->setConfValue(Application::TSAUrl, url);
	});
	connect(ui->rdMIDUUIDCustom, &QRadioButton::toggled, ui->txtMIDUUID, [=](bool checked) {
		ui->txtMIDUUID->setEnabled(checked);
		setValueEx(QStringLiteral("MIDUUID-CUSTOM"), checked, QSettings().contains(QStringLiteral("MIDUUID")));
		setValueEx(QStringLiteral("SIDUUID-CUSTOM"), checked, QSettings().contains(QStringLiteral("SIDUUID")));
	});
	ui->rdMIDUUIDCustom->setChecked(s.value(QStringLiteral("MIDUUID-CUSTOM"), s.contains(QStringLiteral("MIDUUID"))).toBool());
	ui->txtMIDUUID->setText(s.value(QStringLiteral("MIDUUID")).toString());
	connect(ui->txtMIDUUID, &QLineEdit::textChanged, this, [](const QString &text) {
		setValueEx(QStringLiteral("MIDUUID"), text);
		setValueEx(QStringLiteral("SIDUUID"), text);
	});
	connect(ui->helpRevocation, &QToolButton::clicked, this, []{
		QDesktopServices::openUrl(tr("https://www.id.ee/en/article/access-certificate-what-is-it/"));
	});
	connect(ui->helpTimeStamp, &QToolButton::clicked, this, []{
		QDesktopServices::openUrl(tr("https://www.id.ee/en/article/for-organisations-that-sign-large-quantities-of-documents-using-digidoc4-client/"));
	});
	connect(ui->helpMID, &QToolButton::clicked, this, []{
		QDesktopServices::openUrl(tr("https://www.id.ee/en/article/for-organisations-that-sign-large-quantities-of-documents-using-digidoc4-client/"));
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
		setValueEx(QStringLiteral("ProxyConfig"), 0, 0);
	else if(ui->rdProxySystem->isChecked())
		setValueEx(QStringLiteral("ProxyConfig"), 1, 0);
	else if(ui->rdProxyManual->isChecked())
		setValueEx(QStringLiteral("ProxyConfig"), 2, 0);
	Application::setConfValue( Application::ProxyHost, ui->txtProxyHost->text() );
	Application::setConfValue( Application::ProxyPort, ui->txtProxyPort->text() );
	Application::setConfValue( Application::ProxyUser, ui->txtProxyUsername->text() );
	Application::setConfValue( Application::ProxyPass, ui->txtProxyPassword->text() );
	Application::setConfValue( Application::ProxySSL, ui->chkProxyEnableForSSL->isChecked() );
	loadProxy(digidoc::Conf::instance());
	updateProxy();
}

void SettingsDialog::setValueEx(const QString &key, const QVariant &value, const QVariant &def)
{
	if(value == def || (def.isNull() && value.isNull()))
		QSettings().remove(key);
	else
		QSettings().setValue(key, value);
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
		WarningDialog(tr("Invalid password"), this).exec();
		return;
	default:
		WarningDialog(tr("Server access certificate error: %1").arg(p12.errorString()), this).exec();
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
	ui->rdTimeStampDefault->setChecked(true);
	ui->rdMIDUUIDDefault->setChecked(true);
}

void SettingsDialog::showPage(int page)
{
	changePage(ui->pageGroup->button(page));
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
	ui->btnNavSaveLibdigidocpp->setVisible(button == ui->btnMenuDiagnostics &&
		QFile::exists(QStringLiteral("%1/libdigidocpp.log").arg(QDir::tempPath())));
#ifdef Q_OS_WIN
	ui->btnNavFromHistory->setVisible(button == ui->btnMenuGeneral);
#else
	ui->btnNavFromHistory->setVisible(false);
#endif
}

void SettingsDialog::saveFile(const QString &name, const QByteArray &content)
{
	QString filename = FileDialog::getSaveFileName(this, tr("Save as"), QStringLiteral( "%1/%2_%3_%4")
		.arg(QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation), qApp->applicationName(), qApp->applicationVersion(), name),
		tr("Text files (*.txt)") );
	if( filename.isEmpty() )
		return;
	QFile f( filename );
	if(!f.open(QIODevice::WriteOnly|QIODevice::Text) || !f.write(content))
		WarningDialog(tr("Failed write to file!"), this).exec();
}
