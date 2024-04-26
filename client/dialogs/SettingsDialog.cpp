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
#ifdef Q_OS_WIN
#include "CertStore.h"
#endif
#include "CheckConnection.h"
#include "Colors.h"
#include "Diagnostics.h"
#include "FileDialog.h"
#include "QSigner.h"
#include "Settings.h"
#include "Styles.h"
#include "SslCertificate.h"
#include "TokenData.h"
#include "dialogs/CertificateDetails.h"
#include "dialogs/FirstRun.h"
#include "dialogs/WarningDialog.h"
#include "effects/ButtonHoverFilter.h"
#include "effects/Overlay.h"
#include "effects/FadeInNotification.h"

#include "common/Configuration.h"

#include <digidocpp/Conf.h>

#include <QtCore/QJsonObject>
#include <QtCore/QProcess>
#include <QtCore/QStandardPaths>
#include <QtCore/QThreadPool>
#include <QtGui/QDesktopServices>
#include <QtNetwork/QNetworkProxy>
#include <QtWidgets/QInputDialog>

#include <algorithm>

#define qdigidoc4log QStringLiteral("%1/%2.log").arg(QDir::tempPath(), QApplication::applicationName())

SettingsDialog::SettingsDialog(int page, QWidget *parent)
	: QDialog(parent)
	, ui(new Ui::SettingsDialog)
{
	new Overlay(this);

	ui->setupUi(this);
	setWindowFlag(Qt::FramelessWindowHint);
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

	ui->chkShowPrintSummary->setFont(regularFont);
	ui->chkRoleAddressInfo->setFont(regularFont);
	ui->chkLibdigidocppDebug->setFont(regularFont);

	ui->lblGeneralCDoc2->setFont(headerFont);

	ui->chkCdoc2KeyServer->setFont(regularFont);
	ui->lblCdoc2Name->setFont(regularFont);
	ui->lblCdoc2UUID->setFont(regularFont);
	ui->lblCdoc2Fetch->setFont(regularFont);
	ui->lblCdoc2Post->setFont(regularFont);
	ui->cmbCdoc2Name->setFont(regularFont);
	ui->txtCdoc2UUID->setFont(regularFont);
	ui->txtCdoc2Fetch->setFont(regularFont);
	ui->txtCdoc2Post->setFont(regularFont);

	// pageServices
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
		auto *dlg = WarningDialog::show(this, tr("DigiDoc4 Client configuration update was successful."));
		new Overlay(dlg);
#ifdef Q_OS_WIN
		QString path = QApplication::applicationDirPath() + QLatin1String("/id-updater.exe");
		if (QFile::exists(path))
			QProcess::startDetached(path, {});
#endif
	});
#endif

	connect( ui->btNavClose, &QPushButton::clicked, this, &SettingsDialog::accept );
	connect( this, &SettingsDialog::finished, this, &SettingsDialog::close );

	connect(ui->btnCheckConnection, &QPushButton::clicked, this, &SettingsDialog::checkConnection);
	connect(ui->btnFirstRun, &QPushButton::clicked, this, [this] {
		auto *dlg = new FirstRun(this);
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
		Application::updateTSLCache({});
	});
	connect( ui->btnNavUseByDefault, &QPushButton::clicked, this, &SettingsDialog::useDefaultSettings );
	connect( ui->btnNavSaveReport, &QPushButton::clicked, this, [=]{
		saveFile(QStringLiteral("diagnostics.txt"), ui->txtDiagnostics->toPlainText().toUtf8());
	});
	connect(ui->btnNavSaveLibdigidocpp, &QPushButton::clicked, this, [=]{
		Settings::LIBDIGIDOCPP_DEBUG = false;
		QString log = QStringLiteral("%1/libdigidocpp.log").arg(QDir::tempPath());
		saveFile(QStringLiteral("libdigidocpp.txt"), log);
		saveFile(QStringLiteral("qdigidoc4.txt"), qdigidoc4log);
		QFile::remove(log);
		ui->btnNavSaveLibdigidocpp->hide();
	});
#ifdef Q_OS_WIN
	connect(ui->btnNavFromHistory, &QPushButton::clicked, this, [this] {
		// remove certificates from browsing history of Internet Explorer and/or Google Chrome, and do it for all users.
		QList<TokenData> cache = qApp->signer()->cache();
		CertStore s;
		for(const QSslCertificate &c: s.list())
		{
			if(std::any_of(cache.cbegin(), cache.cend(), [&](const TokenData &token) { return token.cert() == c; }))
				continue;
			if(c.issuerInfo(QSslCertificate::CommonName).join(QString()).contains(QStringLiteral("KLASS3-SK"), Qt::CaseInsensitive) ||
				c.issuerInfo(QSslCertificate::Organization).contains(QStringLiteral("SK ID Solutions AS"), Qt::CaseInsensitive))
				s.remove( c );
		}
		WarningDialog::show(this, tr("Redundant certificates have been successfully removed."));
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
		c.subjectInfo(QSslCertificate::CommonName),
		c.expiryDate().toString(QStringLiteral("dd.MM.yyyy")),
		!c.isValid() ? QStringLiteral("<font color='red'>(%1)</font>").arg(tr("expired")) : QString());
}

void SettingsDialog::checkConnection()
{
	QApplication::setOverrideCursor( Qt::WaitCursor );
	saveProxy();
	if(CheckConnection connection; !connection.check())
	{
		Application::restoreOverrideCursor();
		auto *notification = new FadeInNotification(this,
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
		auto *notification = new FadeInNotification(this,
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
	updateDiagnostics();
}

void SettingsDialog::initFunctionality()
{
	// pageGeneral
	selectLanguage();
	connect(ui->langGroup, qOverload<QAbstractButton*>(&QButtonGroup::buttonClicked), this,
		[this](QAbstractButton *button){ retranslate(button->property("lang").toString()); });

	ui->chkShowPrintSummary->setChecked(Settings::SHOW_PRINT_SUMMARY);
	connect(ui->chkShowPrintSummary, &QCheckBox::toggled, this, &SettingsDialog::togglePrinting);
	connect(ui->chkShowPrintSummary, &QCheckBox::toggled, this, [](bool checked) {
		Settings::SHOW_PRINT_SUMMARY = checked;
	});
	ui->chkRoleAddressInfo->setChecked(Settings::SHOW_ROLE_ADDRESS_INFO);
	connect(ui->chkRoleAddressInfo, &QCheckBox::toggled, this, [](bool checked) {
		Settings::SHOW_ROLE_ADDRESS_INFO = checked;
	});

#ifdef Q_OS_MAC
	ui->lblDefaultDirectory->hide();
	ui->rdGeneralSameDirectory->hide();
	ui->txtGeneralDirectory->hide();
	ui->btGeneralChooseDirectory->hide();
	ui->rdGeneralSpecifyDirectory->hide();
#else
	connect(ui->btGeneralChooseDirectory, &QPushButton::clicked, this, [=]{
		QString dir = FileDialog::getExistingDirectory(this, tr("Select folder"), Settings::DEFAULT_DIR);
		if(!dir.isEmpty())
		{
			ui->rdGeneralSpecifyDirectory->setChecked(true);
			Settings::DEFAULT_DIR = dir;
			ui->txtGeneralDirectory->setText(dir);
		}
	});
	connect(ui->rdGeneralSpecifyDirectory, &QRadioButton::toggled, this, [=](bool enable) {
		ui->btGeneralChooseDirectory->setVisible(enable);
		ui->txtGeneralDirectory->setVisible(enable);
		if(!enable)
			ui->txtGeneralDirectory->clear();
	});
	ui->txtGeneralDirectory->setText(Settings::DEFAULT_DIR);
	if(ui->txtGeneralDirectory->text().isEmpty())
		ui->rdGeneralSameDirectory->setChecked(true);
	connect(ui->txtGeneralDirectory, &QLineEdit::textChanged, this, [](const QString &text) {
		Settings::DEFAULT_DIR = text;
	});
#endif
	ui->wgtCDoc2->hide();
#if 0
	ui->chkCdoc2KeyServer->setChecked(Settings::CDOC2_USE_KEYSERVER);
	ui->cmbCdoc2Name->setEnabled(ui->chkCdoc2KeyServer->isChecked());
	connect(ui->chkCdoc2KeyServer, &QCheckBox::toggled, this, [this](bool checked) {
		Settings::CDOC2_USE_KEYSERVER = checked;
		ui->cmbCdoc2Name->setEnabled(checked);
	});
#ifdef CONFIG_URL
	QJsonObject list = Application::confValue(QLatin1String("CDOC2-CONF")).toObject();
	auto setCDoc2Values = [this, list](const QString &key) {
		ui->txtCdoc2UUID->setText(key);
		QJsonObject data = list.value(key).toObject();
		ui->txtCdoc2Fetch->setText(data.value(QLatin1String("FETCH")).toString(Settings::CDOC2_GET));
		ui->txtCdoc2Post->setText(data.value(QLatin1String("POST")).toString(Settings::CDOC2_POST));
	};
	for(QJsonObject::const_iterator i = list.constBegin(); i != list.constEnd(); ++i)
		ui->cmbCdoc2Name->addItem(i.value().toObject().value(QLatin1String("NAME")).toString(), i.key());
	if(Settings::CDOC2_GET.isSet() || Settings::CDOC2_POST.isSet())
		ui->cmbCdoc2Name->addItem(QStringLiteral("Custom"), QStringLiteral("custom"));
	connect(ui->cmbCdoc2Name, qOverload<int>(&QComboBox::currentIndexChanged), this, [this, setCDoc2Values] (int index) {
		QString key = ui->cmbCdoc2Name->itemData(index).toString();
		Settings::CDOC2_DEFAULT_KEYSERVER = key;
		setCDoc2Values(key);
	});
	QString cdoc2Service = Settings::CDOC2_DEFAULT_KEYSERVER;
	ui->cmbCdoc2Name->setCurrentIndex(ui->cmbCdoc2Name->findData(cdoc2Service));
	setCDoc2Values(cdoc2Service);
#else
	ui->cmbCdoc2Name->addItem(QStringLiteral("Default"));
	ui->txtCdoc2UUID->setText(QStringLiteral("default"));
	ui->txtCdoc2Fetch->setText(QStringLiteral(CDOC2_GET_URL));
	ui->txtCdoc2Post->setText(QStringLiteral(CDOC2_POST_URL));
#endif
#endif

	// pageProxy
	connect(this, &SettingsDialog::finished, this, &SettingsDialog::saveProxy);
	setProxyEnabled();
	connect( ui->rdProxyNone, &QRadioButton::toggled, this, &SettingsDialog::setProxyEnabled );
	connect( ui->rdProxySystem, &QRadioButton::toggled, this, &SettingsDialog::setProxyEnabled );
	connect( ui->rdProxyManual, &QRadioButton::toggled, this, &SettingsDialog::setProxyEnabled );
	switch(Settings::PROXY_CONFIG)
	{
	case Settings::ProxySystem: ui->rdProxySystem->setChecked(true); break;
	case Settings::ProxyManual: ui->rdProxyManual->setChecked(true); break;
	default: ui->rdProxyNone->setChecked(true); break;
	}

	updateProxy();

	// pageServices - TimeStamp
	ui->rdTimeStampDefault->setDisabled(Settings::TSA_URL_CUSTOM.isLocked());
	ui->rdTimeStampCustom->setEnabled(ui->rdTimeStampDefault->isEnabled());
	ui->rdTimeStampCustom->setChecked(Settings::TSA_URL_CUSTOM);
	ui->txtTimeStamp->setReadOnly(Settings::TSA_URL.isLocked());
	ui->txtTimeStamp->setEnabled(ui->rdTimeStampCustom->isChecked());
	ui->txtTimeStamp->setPlaceholderText(Application::confValue(Settings::TSA_URL.KEY).toString());
	QString TSA_URL = Settings::TSA_URL.value(Application::confValue(Application::TSAUrl));
	ui->txtTimeStamp->setText(ui->txtTimeStamp->placeholderText() == TSA_URL ? QString() : std::move(TSA_URL));
	ui->wgtTSACert->setDisabled(Settings::TSA_CERT.isLocked());
	ui->wgtTSACert->setVisible(ui->rdTimeStampCustom->isChecked());
	connect(ui->rdTimeStampCustom, &QRadioButton::toggled, ui->txtTimeStamp, [this](bool checked) {
		ui->txtTimeStamp->setEnabled(checked);
		ui->wgtTSACert->setVisible(checked);
		Settings::TSA_URL_CUSTOM = checked;
	});
	connect(ui->txtTimeStamp, &QLineEdit::textChanged, this, [this](const QString &url) {
		Settings::TSA_URL = url;
		if(url.isEmpty())
		{
			Settings::TSA_CERT.clear();
			updateTSACert(QSslCertificate());
		}
	});
	connect(ui->helpTimeStamp, &QToolButton::clicked, this, []{
		QDesktopServices::openUrl(tr("https://www.id.ee/en/article/for-organisations-that-sign-large-quantities-of-documents-using-digidoc4-client/"));
	});
	connect(ui->btInstallTSACert, &QPushButton::clicked, this, [this] {
		QSslCertificate cert = selectCert(tr("Select Time-Stamping server certificate"),
			tr("Time-Stamping service SSL certificate"));
		if(cert.isNull())
			return;
		Settings::TSA_CERT = cert.toDer().toBase64();
		updateTSACert(cert);
	});
	updateTSACert(QSslCertificate(QByteArray::fromBase64(Settings::TSA_CERT), QSsl::Der));

	// pageServices - MID
	ui->rdMIDUUIDDefault->setDisabled(Settings::MID_UUID_CUSTOM.isLocked());
	ui->rdMIDUUIDCustom->setEnabled(ui->rdMIDUUIDDefault->isEnabled());
	ui->rdMIDUUIDCustom->setChecked(Settings::MID_UUID_CUSTOM);
	ui->txtMIDUUID->setReadOnly(Settings::MID_UUID.isLocked());
	ui->txtMIDUUID->setEnabled(ui->rdMIDUUIDCustom->isChecked());
	ui->txtMIDUUID->setText(Settings::MID_UUID);
	connect(ui->rdMIDUUIDCustom, &QRadioButton::toggled, ui->txtMIDUUID, [=](bool checked) {
		ui->txtMIDUUID->setEnabled(checked);
		Settings::MID_UUID_CUSTOM = checked;
		Settings::SID_UUID_CUSTOM = checked;
	});
	connect(ui->txtMIDUUID, &QLineEdit::textChanged, this, [](const QString &text) {
		Settings::MID_UUID = text;
		Settings::SID_UUID = text;
	});
	connect(ui->helpMID, &QToolButton::clicked, this, []{
		QDesktopServices::openUrl(tr("https://www.id.ee/en/article/for-organisations-that-sign-large-quantities-of-documents-using-digidoc4-client/"));
	});

	// pageValidation - SiVa
	ui->rdSiVaDefault->setDisabled(Settings::SIVA_URL_CUSTOM.isLocked());
	ui->rdSiVaCustom->setEnabled(ui->rdSiVaDefault->isEnabled());
	ui->rdSiVaCustom->setChecked(Settings::SIVA_URL_CUSTOM);
	ui->txtSiVa->setReadOnly(Settings::SIVA_URL.isLocked());
	ui->txtSiVa->setEnabled(ui->rdSiVaCustom->isChecked());
	ui->txtSiVa->setPlaceholderText(Application::confValue(Settings::SIVA_URL.KEY).toString());
	QString SIVA_URL = Settings::SIVA_URL.value(Application::confValue(Application::SiVaUrl));
	ui->txtSiVa->setText(ui->txtSiVa->placeholderText() == SIVA_URL ? QString() : std::move(SIVA_URL));
	ui->wgtSiVaCert->setDisabled(Settings::SIVA_CERT.isLocked());
	ui->wgtSiVaCert->setVisible(ui->rdSiVaCustom->isChecked());
	connect(ui->rdSiVaCustom, &QRadioButton::toggled, ui->txtSiVa, [this](bool checked) {
		ui->txtSiVa->setEnabled(checked);
		ui->wgtSiVaCert->setVisible(checked);
		Settings::SIVA_URL_CUSTOM = checked;
	});
	connect(ui->txtSiVa, &QLineEdit::textChanged, this, [this](const QString &url) {
		Settings::SIVA_URL = url;
		if(url.isEmpty())
		{
			Settings::SIVA_CERT.clear();
			updateSiVaCert(QSslCertificate());
		}
	});
	connect(ui->helpSiVa, &QToolButton::clicked, this, []{
		QDesktopServices::openUrl(tr("https://www.id.ee/en/article/configuring-the-siva-validation-service-in-the-digidoc4-client/"));
	});
	connect(ui->btInstallSiVaCert, &QPushButton::clicked, this, [this] {
		QSslCertificate cert = selectCert(tr("Select SiVa server certificate"),
			tr("Digital Signature Validation Service SiVa SSL certificate"));
		if(cert.isNull())
			return;
		Settings::SIVA_CERT = cert.toDer().toBase64();
		updateSiVaCert(cert);
	});
	updateSiVaCert(QSslCertificate(QByteArray::fromBase64(Settings::SIVA_CERT), QSsl::Der));

	// pageDiagnostics
	ui->chkLibdigidocppDebug->setChecked(Settings::LIBDIGIDOCPP_DEBUG);
	connect(ui->chkLibdigidocppDebug, &QCheckBox::toggled, this, [this](bool checked) {
		Settings::LIBDIGIDOCPP_DEBUG = checked;
		if(!checked)
		{
			QFile::remove(qdigidoc4log);
			return;
		}
		if(QFile f(qdigidoc4log); f.open(QFile::WriteOnly|QFile::Truncate))
			f.write({});
#ifdef Q_OS_MAC
		WarningDialog::show(this, tr("Restart DigiDoc4 Client to activate logging. Read more "
			"<a href=\"https://www.id.ee/en/article/log-file-generation-in-digidoc4-client/\">here</a>."));
#else
		auto *dlg = WarningDialog::show(this, tr("Restart DigiDoc4 Client to activate logging. Read more "
			"<a href=\"https://www.id.ee/en/article/log-file-generation-in-digidoc4-client/\">here</a>. Restart now?"));
		dlg->setCancelText(WarningDialog::NO);
		dlg->addButton(WarningDialog::YES, 1);
		connect(dlg, &WarningDialog::finished, qApp, [](int result) {
			if(result == 1) {
				qApp->setProperty("restart", true);
				QApplication::quit();
			}
		});
#endif
	});
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
	QFile file(FileDialog::getOpenFileName(this, label, {}, QStringLiteral("%1 (*.crt *.cer *.pem)").arg(format)));
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
		button->setChecked(button->property("lang").toString() == Settings::LANGUAGE);
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
}

void SettingsDialog::updateVersion()
{
	ui->txtNavVersion->setText(tr("%1 version %2, released %3")
		.arg(tr("DigiDoc4 Client"), QApplication::applicationVersion(), QStringLiteral(BUILD_DATE)));
}

void SettingsDialog::saveProxy()
{
	if(ui->rdProxyNone->isChecked())
		Settings::PROXY_CONFIG = Settings::ProxyNone;
	else if(ui->rdProxySystem->isChecked())
		Settings::PROXY_CONFIG = Settings::ProxySystem;
	else if(ui->rdProxyManual->isChecked())
		Settings::PROXY_CONFIG = Settings::ProxyManual;
	Application::setConfValue( Application::ProxyHost, ui->txtProxyHost->text() );
	Application::setConfValue( Application::ProxyPort, ui->txtProxyPort->text() );
	Application::setConfValue( Application::ProxyUser, ui->txtProxyUsername->text() );
	Application::setConfValue( Application::ProxyPass, ui->txtProxyPassword->text() );
	loadProxy(digidoc::Conf::instance());
	updateProxy();
}

void SettingsDialog::loadProxy( const digidoc::Conf *conf )
{
	switch(Settings::PROXY_CONFIG)
	{
	case Settings::ProxyNone:
		QNetworkProxyFactory::setUseSystemConfiguration(false);
		QNetworkProxy::setApplicationProxy({});
		break;
	case Settings::ProxySystem:
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
	auto *worker = new Diagnostics();
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

void SettingsDialog::useDefaultSettings()
{
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
	QString filename = FileDialog::getSaveFileName(this, tr("Save as"), QStringLiteral( "%1/%2_%3_%4").arg(
		QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation),
		QApplication::applicationName(),
		QApplication::applicationVersion(),
		name),
		tr("Text files (*.txt)"));
	if( filename.isEmpty() )
		return;
	if(QFile f(filename); !f.open(QIODevice::WriteOnly|QIODevice::Text) || !f.write(content))
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
