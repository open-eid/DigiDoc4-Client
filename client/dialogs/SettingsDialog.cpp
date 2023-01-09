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

#include <QtCore/QJsonObject>
#include <QtCore/QProcess>
#include <QtCore/QSettings>
#include <QtCore/QStandardPaths>
#include <QtCore/QThreadPool>
#include <QtGui/QDesktopServices>
#include <QtNetwork/QNetworkProxy>
#include <QtWidgets/QInputDialog>

#include <algorithm>

#define qdigidoc4log QStringLiteral("%1/%2.log").arg(QDir::tempPath(), qApp->applicationName())

SettingsDialog::SettingsDialog(QWidget *parent)
	: QDialog(parent)
	, ui(new Ui::SettingsDialog)
{
	new Overlay(this, parent->topLevelWidget());

	ui->setupUi(this);
	setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint);
	move(parent->geometry().center() - geometry().center());
	for(QLineEdit *w: findChildren<QLineEdit*>())
		w->setAttribute(Qt::WA_MacShowFocusRect, false);
	for(QRadioButton *w: findChildren<QRadioButton*>())
		w->setAttribute(Qt::WA_MacShowFocusRect, false);
	for(QCheckBox *w: findChildren<QCheckBox*>())
		w->setAttribute(Qt::WA_MacShowFocusRect, false);

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
	ui->btnMenuValidation->setFont(condensed12);
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

	ui->chkShowPrintSummary->setFont(regularFont);
	ui->chkRoleAddressInfo->setFont(regularFont);
	ui->chkLibdigidocppDebug->setFont(regularFont);

	// pageServices
	ui->lblRevocation->setFont(headerFont);
	ui->lblAccessCert->setFont(regularFont);
	ui->txtAccessCert->setFont(regularFont);
	ui->btInstallManually->setFont(condensed12);
	ui->btShowCertificate->setFont(condensed12);
#if defined(Q_OS_UNIX) && !defined(Q_OS_MAC)
	ui->btInstallManually->setStyleSheet("background-color: #d3d3d3");
	ui->btShowCertificate->setStyleSheet("background-color: #d3d3d3");
#endif
	ui->chkIgnoreAccessCert->setFont(regularFont);
	ui->lblTimeStamp->setFont(headerFont);
	ui->rdTimeStampDefault->setFont(regularFont);
	ui->rdTimeStampCustom->setFont(regularFont);
	ui->txtTimeStamp->setFont(regularFont);
	ui->lblTSACert->setFont(regularFont);
	ui->txtTSACert->setFont(regularFont);
	ui->btInstallTSACert->setFont(condensed12);
	ui->btShowTSACert->setFont(condensed12);
#if defined(Q_OS_UNIX) && !defined(Q_OS_MAC)
	ui->btInstallTSACert->setStyleSheet("background-color: #d3d3d3");
	ui->btShowTSACert->setStyleSheet("background-color: #d3d3d3");
#endif
	ui->lblMID->setFont(headerFont);
	ui->rdMIDUUIDDefault->setFont(regularFont);
	ui->rdMIDUUIDCustom->setFont(regularFont);
	ui->txtMIDUUID->setFont(regularFont);
	ui->helpRevocation->installEventFilter(new ButtonHoverFilter(QStringLiteral(":/images/icon_Abi.svg"), QStringLiteral(":/images/icon_Abi_hover.svg"), this));
	ui->helpTimeStamp->installEventFilter(new ButtonHoverFilter(QStringLiteral(":/images/icon_Abi.svg"), QStringLiteral(":/images/icon_Abi_hover.svg"), this));
	ui->helpMID->installEventFilter(new ButtonHoverFilter(QStringLiteral(":/images/icon_Abi.svg"), QStringLiteral(":/images/icon_Abi_hover.svg"), this));

	// pageValidation
	ui->lblSiVa->setFont(headerFont);
	ui->txtSiVa->setFont(regularFont);
	ui->rdSiVaDefault->setFont(regularFont);
	ui->rdSiVaCustom->setFont(regularFont);
	ui->lblSiVaCert->setFont(regularFont);
	ui->txtSiVaCert->setFont(regularFont);
	ui->btInstallSiVaCert->setFont(condensed12);
	ui->btShowSiVaCert->setFont(condensed12);
#if defined(Q_OS_UNIX) && !defined(Q_OS_MAC)
	ui->btInstallSiVaCert->setStyleSheet("background-color: #d3d3d3");
	ui->btShowSiVaCert->setStyleSheet("background-color: #d3d3d3");
#endif
	ui->helpSiVa->installEventFilter(new ButtonHoverFilter(QStringLiteral(":/images/icon_Abi.svg"), QStringLiteral(":/images/icon_Abi_hover.svg"), this));

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
	connect(qApp->conf(), &Configuration::finished, this, [=](bool /*update*/, const QString &error){
		if(!error.isEmpty()) {
			WarningDialog::show(this, tr("Checking updates has failed.") + "<br />" + tr("Please try again."), error);
			return;
		}
		WarningDialog(tr("DigiDoc4 Client configuration update was successful."), this).exec();
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
		FirstRun *dlg = new FirstRun(this);
		connect(dlg, &FirstRun::langChanged, this, [this](const QString &lang) {
			retranslate(lang);
			selectLanguage();
		});
		dlg->open();
	});
	connect(ui->btnRefreshConfig, &QPushButton::clicked, this, [] {
#ifdef CONFIG_URL
		qApp->conf()->update(true);
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
		saveFile(QStringLiteral("libdigidocpp.txt"),
			QStringLiteral("%1/libdigidocpp.log").arg(QDir::tempPath()));
		saveFile(QStringLiteral("qdigidoc4.txt"), qdigidoc4log);
		ui->btnNavSaveLibdigidocpp->hide();
	});
#ifdef Q_OS_WIN
	connect(ui->btnNavFromHistory, &QPushButton::clicked, this, [] {
		// remove certificates (having %ESTEID% text) from browsing history of Internet Explorer and/or Google Chrome, and do it for all users.
		QList<TokenData> cache = qApp->signer()->cache();
		CertStore s;
		const QList<QSslCertificate> certificates = s.list();
		for(const QSslCertificate &c: certificates)
		{
			if(std::any_of(cache.cbegin(), cache.cend(), [&](const TokenData &token) { return token.cert() == c; }))
				continue;
			if(c.subjectInfo(QSslCertificate::Organization).join(QString()).contains(QStringLiteral("ESTEID"), Qt::CaseInsensitive) ||
				c.issuerInfo(QSslCertificate::CommonName).join(QString()).contains(QStringLiteral("KLASS3-SK"), Qt::CaseInsensitive) ||
				c.issuerInfo(QSslCertificate::Organization).contains(QStringLiteral("SK ID Solutions AS"), Qt::CaseInsensitive))
				s.remove( c );
		}
		qApp->showWarning( tr("Redundant certificates have been successfully removed.") );
	});
#endif

	ui->pageGroup->setId(ui->btnMenuGeneral, GeneralSettings);
	ui->pageGroup->setId(ui->btnMenuCertificate, SigningSettings);
	ui->pageGroup->setId(ui->btnMenuValidation, ValidationSettings);
	ui->pageGroup->setId(ui->btnMenuProxy, NetworkSettings);
	ui->pageGroup->setId(ui->btnMenuDiagnostics, DiagnosticsSettings);
	ui->pageGroup->setId(ui->btnMenuInfo, LicenseSettings);
	connect(ui->pageGroup, qOverload<QAbstractButton*>(&QButtonGroup::buttonClicked), this, &SettingsDialog::changePage);

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

QString SettingsDialog::certInfo(const SslCertificate &c)
{
	return tr("Issued to: %1<br />Valid to: %2 %3").arg(
		CertificateDetails::decodeCN(c.subjectInfo(QSslCertificate::CommonName)),
		c.expiryDate().toString(QStringLiteral("dd.MM.yyyy")),
		!c.isValid() ? QStringLiteral("<font color='red'>(%1)</font>").arg(tr("expired")) : QString());
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
		notification->start(error, 750, 3000, 1200);
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
	connect(ui->langGroup, qOverload<QAbstractButton*>(&QButtonGroup::buttonClicked), this,
		[this](QAbstractButton *button){ retranslate(button->property("lang").toString()); });

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

	// pageServices - Access Cert
	updateCert();
	connect(ui->btShowCertificate, &QPushButton::clicked, this, [this] {
		CertificateDetails::showCertificate(SslCertificate(AccessCert::cert()), this);
	});
	connect(ui->btInstallManually, &QPushButton::clicked, this, &SettingsDialog::installCert);
	ui->chkIgnoreAccessCert->setChecked(Application::confValue(Application::PKCS12Disable, false).toBool());
	connect(ui->chkIgnoreAccessCert, &QCheckBox::toggled, this, [](bool checked) {
		Application::setConfValue(Application::PKCS12Disable, checked);
	});
	connect(ui->helpRevocation, &QToolButton::clicked, this, []{
		QDesktopServices::openUrl(tr("https://www.id.ee/en/article/access-certificate-what-is-it/"));
	});

	// pageServices - TimeStamp
	connect(ui->rdTimeStampCustom, &QRadioButton::toggled, ui->txtTimeStamp, [this](bool checked) {
		ui->txtTimeStamp->setEnabled(checked);
		ui->wgtTSACert->setVisible(checked);
		setValueEx(QStringLiteral("TSA-URL-CUSTOM"), checked, QSettings().contains(QStringLiteral("TSA-URL")));
	});
	ui->rdTimeStampCustom->setChecked(s.value(QStringLiteral("TSA-URL-CUSTOM"), s.contains(QStringLiteral("TSA-URL"))).toBool());
	ui->wgtTSACert->setVisible(ui->rdTimeStampCustom->isChecked());
#ifdef CONFIG_URL
	ui->txtTimeStamp->setPlaceholderText(qApp->conf()->object().value(QStringLiteral("TSA-URL")).toString());
#endif
	QString TSA_URL = s.value(QStringLiteral("TSA-URL"), qApp->confValue(Application::TSAUrl)).toString();
	ui->txtTimeStamp->setText(ui->txtTimeStamp->placeholderText() == TSA_URL ? QString() : TSA_URL);
	connect(ui->txtTimeStamp, &QLineEdit::textChanged, this, [this](const QString &url) {
		qApp->setConfValue(Application::TSAUrl, url);
		if(url.isEmpty())
		{
			QSettings().remove(QStringLiteral("TSA-CERT"));
			updateTSACert(QSslCertificate());
		}
	});
	connect(ui->helpTimeStamp, &QToolButton::clicked, this, []{
		QDesktopServices::openUrl(tr("https://www.id.ee/en/article/for-organisations-that-sign-large-quantities-of-documents-using-digidoc4-client/"));
	});
	connect(ui->btInstallTSACert, &QPushButton::clicked, this, [this] {
		QSslCertificate cert = selectCert(tr("Select Time-Stamping server certificate"),
			QStringLiteral("%1 (*.crt *.cer *.pem)").arg(tr("Time-Stamping service SSL certificate")));
		if(cert.isNull())
			return;
		QSettings().setValue(QStringLiteral("TSA-CERT"), cert.toDer().toBase64());
		updateTSACert(cert);
	});
	updateTSACert(QSslCertificate(QByteArray::fromBase64(s.value(QStringLiteral("TSA-CERT")).toByteArray()), QSsl::Der));

	// pageServices - MID
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
	connect(ui->helpMID, &QToolButton::clicked, this, []{
		QDesktopServices::openUrl(tr("https://www.id.ee/en/article/for-organisations-that-sign-large-quantities-of-documents-using-digidoc4-client/"));
	});

	// pageValidation - SiVa
	connect(ui->rdSiVaCustom, &QRadioButton::toggled, ui->txtSiVa, [this](bool checked) {
		ui->txtSiVa->setEnabled(checked);
		ui->wgtSiVaCert->setVisible(checked);
		setValueEx(QStringLiteral("SIVA-URL-CUSTOM"), checked, QSettings().contains(QStringLiteral("SIVA-URL")));
	});
	ui->rdSiVaCustom->setChecked(s.value(QStringLiteral("SIVA-URL-CUSTOM"), s.contains(QStringLiteral("SIVA-URL"))).toBool());
	ui->wgtSiVaCert->setVisible(ui->rdSiVaCustom->isChecked());
#ifdef CONFIG_URL
	ui->txtSiVa->setPlaceholderText(qApp->conf()->object().value(QStringLiteral("SIVA-URL")).toString());
#endif
	QString SIVA_URL = s.value(QStringLiteral("SIVA-URL"), qApp->confValue(Application::SiVaUrl)).toString();
	ui->txtSiVa->setText(ui->txtSiVa->placeholderText() == SIVA_URL ? QString() : SIVA_URL);
	connect(ui->txtSiVa, &QLineEdit::textChanged, this, [this](const QString &url) {
		qApp->setConfValue(Application::SiVaUrl, url);
		if(url.isEmpty())
		{
			QSettings().remove(QStringLiteral("SIVA-CERT"));
			updateSiVaCert(QSslCertificate());
		}
	});
	connect(ui->helpSiVa, &QToolButton::clicked, this, []{
		QDesktopServices::openUrl(tr("https://www.id.ee/en/article/configuring-the-siva-validation-service-in-the-digidoc4-client/"));
	});
	connect(ui->btInstallSiVaCert, &QPushButton::clicked, this, [this] {
		QSslCertificate cert = selectCert(tr("Select SiVa server certificate"),
			QStringLiteral("%1 (*.crt *.cer *.pem)").arg(tr("Digital Signature Validation Service SiVa SSL certificate")));
		if(cert.isNull())
			return;
		QSettings().setValue(QStringLiteral("SIVA-CERT"), cert.toDer().toBase64());
		updateSiVaCert(cert);
	});
	updateSiVaCert(QSslCertificate(QByteArray::fromBase64(s.value(QStringLiteral("SIVA-CERT")).toByteArray()), QSsl::Der));

	// pageDiagnostics
	ui->chkLibdigidocppDebug->setChecked(s.value(QStringLiteral("LibdigidocppDebug"), false).toBool());
	connect(ui->chkLibdigidocppDebug, &QCheckBox::toggled, this, [this](bool checked) {
		setValueEx(QStringLiteral("LibdigidocppDebug"), checked, false);
		if(!checked)
		{
			QFile::remove(qdigidoc4log);
			return;
		}
		QFile f(qdigidoc4log);
		if(f.open(QFile::WriteOnly|QFile::Truncate))
			f.write({});
#ifdef Q_OS_MAC
		WarningDialog::show(this, tr("Restart DigiDoc4 Client to activate logging. Read more "
			"<a href=\"https://www.id.ee/en/article/log-file-generation-in-digidoc4-client/\">here</a>."));
#else
		WarningDialog dlg(tr("Restart DigiDoc4 Client to activate logging. Read more "
							 "<a href=\"https://www.id.ee/en/article/log-file-generation-in-digidoc4-client/\">here</a>. Restart now?"), this);
		dlg.setCancelText(tr("NO"));
		dlg.addButton(tr("YES"), 1) ;
		if(dlg.exec() == 1) {
			qApp->setProperty("restart", true);
			qApp->quit();
		}
#endif
	});
}

void SettingsDialog::updateCert()
{
	QSslCertificate c = AccessCert::cert();
	if(!c.isNull())
		ui->txtAccessCert->setText(certInfo(c));
	else
		ui->txtAccessCert->setText(QStringLiteral("<b>%1</b>")
			.arg(tr("Server access certificate is not installed.")));
	ui->btShowCertificate->setDisabled(c.isNull());
}

void SettingsDialog::updateCert(const QSslCertificate &c, QPushButton *btn, CertLabel *lbl)
{
	disconnect(btn, &QPushButton::clicked, nullptr, nullptr);
	btn->setHidden(c.isNull());
	lbl->setHidden(c.isNull());
	if(c.isNull())
		return;
	lbl->setCert(c);
	connect(btn, &QPushButton::clicked, this, [this, c] {
		CertificateDetails::showCertificate(c, this);
	});
}

void SettingsDialog::updateSiVaCert(const QSslCertificate &c)
{
	updateCert(c, ui->btShowSiVaCert, ui->txtSiVaCert);
}

void SettingsDialog::updateTSACert(const QSslCertificate &c)
{
	updateCert(c, ui->btShowTSACert, ui->txtTSACert);
}

QSslCertificate SettingsDialog::selectCert(const QString &label, const QString &format)
{
	QFile file(FileDialog::getOpenFileName(this, label, {}, format));
	if(!file.open(QFile::ReadOnly))
		return QSslCertificate();
	QSslCertificate cert(&file, QSsl::Pem);
	if(!cert.isNull())
		return cert;
	file.seek(0);
	return QSslCertificate(&file, QSsl::Der);
}

void SettingsDialog::selectLanguage()
{
	for(QAbstractButton *button: ui->langGroup->buttons())
		button->setChecked(button->property("lang").toString() == Common::language());
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
		.arg(tr("DigiDoc4 Client"), qApp->applicationVersion(), QStringLiteral(BUILD_DATE)));
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
	bool valueIsNull = value.type() == QVariant::String ? value.toString().isEmpty() : value.isNull();
	if(value == def || (def.isNull() && valueIsNull))
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
		QNetworkProxy::setApplicationProxy({});
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
	ui->btnNavSaveReport->setDisabled(true);

	QApplication::setOverrideCursor( Qt::WaitCursor );
	Diagnostics *worker = new Diagnostics();
	connect(worker, &Diagnostics::update, ui->txtDiagnostics, &QTextBrowser::insertHtml, Qt::QueuedConnection);
	connect(worker, &Diagnostics::destroyed, this, [=]{
		ui->txtDiagnostics->setEnabled(true);
		ui->txtDiagnostics->moveCursor(QTextCursor::Start);
		ui->txtDiagnostics->ensureCursorVisible();
		ui->btnNavSaveReport->setEnabled(true);
		QApplication::restoreOverrideCursor();
	}, Qt::QueuedConnection);
	QThreadPool::globalInstance()->start( worker );
}

void SettingsDialog::installCert()
{
	QFile file(FileDialog::getOpenFileName(this, tr("Select server access certificate"), {},
		tr("Server access certificates (*.p12 *.p12d *.pfx)") ) );
	if(!file.open(QFile::ReadOnly))
		return;
	QString pass = QInputDialog::getText( this, tr("Password"),
		tr("Enter server access certificate password."), QLineEdit::Password );
	if(pass.isEmpty())
		return;
	PKCS12Certificate p12(&file, pass);
	switch(p12.error())
	{
	case PKCS12Certificate::NullError: break;
	case PKCS12Certificate::InvalidPasswordError:
		WarningDialog::show(this, tr("Invalid password"));
		return;
	default:
		WarningDialog::show(this, tr("Server access certificate error: %1").arg(p12.errorString()));
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
	ui->rdSiVaDefault->setChecked(true);
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
	ui->btnNavUseByDefault->setVisible(button == ui->btnMenuCertificate || button == ui->btnMenuValidation);
	ui->btnFirstRun->setVisible(button == ui->btnMenuGeneral);
	ui->btnRefreshConfig->setVisible(button == ui->btnMenuGeneral);
	ui->btnCheckConnection->setVisible(button == ui->btnMenuProxy);
	ui->btnNavSaveReport->setVisible(button == ui->btnMenuDiagnostics);
	ui->btnNavSaveLibdigidocpp->setVisible(button == ui->btnMenuDiagnostics &&
		QFile::exists(QStringLiteral("%1/libdigidocpp.log").arg(QDir::tempPath())));
#ifdef Q_OS_WIN
	ui->btnNavFromHistory->setVisible(button == ui->btnMenuGeneral);
#else
	ui->btnNavFromHistory->hide();
#endif
}

void SettingsDialog::saveFile(const QString &name, const QString &path)
{
	QFile f(path);
	if(f.open(QFile::ReadOnly|QFile::Text))
		saveFile(name, f.readAll());
	f.remove();
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
		WarningDialog::show(this, tr("Failed write to file!"));
}



void CertLabel::setCert(const QSslCertificate &cert)
{
	setText(SettingsDialog::certInfo(cert));
	setProperty("cert", QVariant::fromValue(cert));
}

bool CertLabel::event(QEvent *event)
{
	if(event->type() == QEvent::LanguageChange)
		setText(SettingsDialog::certInfo(property("cert").value<QSslCertificate>()));
	return QLabel::event(event);
}
