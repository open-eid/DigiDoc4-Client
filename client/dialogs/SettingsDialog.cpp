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
#include "CheckConnection.h"
#include "Configuration.h"
#include "Diagnostics.h"
#include "FileDialog.h"
#include "QSigner.h"
#include "Settings.h"
#include "SslCertificate.h"
#include "TokenData.h"
#include "dialogs/CertificateDetails.h"
#include "dialogs/FirstRun.h"
#include "dialogs/WarningDialog.h"
#include "effects/Overlay.h"
#include "effects/FadeInNotification.h"

#include <digidocpp/XmlConf.h>

#include <QtCore/QJsonObject>
#include <QtCore/QProcess>
#include <QtCore/QStandardPaths>
#include <QtCore/QThreadPool>
#include <QtGui/QDesktopServices>
#include <QtNetwork/QNetworkProxy>
#include <QtWidgets/QInputDialog>
#include <QtWidgets/QMessageBox>

#include <algorithm>

#ifdef Q_OS_WIN
#include <qt_windows.h>
#include <WinCrypt.h>

using namespace Qt::StringLiterals;
#endif

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
	for(QToolButton *b: findChildren<QToolButton*>())
		b->setCursor(Qt::PointingHandCursor);
	for(QPushButton *b: findChildren<QPushButton*>())
	{
		b->setCursor(Qt::PointingHandCursor);
		b->setAutoDefault(false);
	}

	// pageGeneral
	selectLanguage();
	connect(ui->langGroup, qOverload<QAbstractButton*>(&QButtonGroup::buttonClicked), this,
		[this](QAbstractButton *button){ retranslate(button->property("lang").toString()); });

	ui->chkShowPrintSummary->setChecked(Settings::SHOW_PRINT_SUMMARY);
	connect(ui->chkShowPrintSummary, &QCheckBox::toggled, this, &SettingsDialog::togglePrinting);
	connect(ui->chkShowPrintSummary, &QCheckBox::toggled, this, Settings::SHOW_PRINT_SUMMARY);
	ui->chkRoleAddressInfo->setChecked(Settings::SHOW_ROLE_ADDRESS_INFO);
	connect(ui->chkRoleAddressInfo, &QCheckBox::toggled, this, Settings::SHOW_ROLE_ADDRESS_INFO);

	// pageServices - TimeStamp
	ui->rdTimeStampDefault->setDisabled(Settings::TSA_URL_CUSTOM.isLocked());
	ui->rdTimeStampCustom->setEnabled(ui->rdTimeStampDefault->isEnabled());
	ui->rdTimeStampCustom->setChecked(Settings::TSA_URL_CUSTOM);
	ui->txtTimeStamp->setReadOnly(Settings::TSA_URL.isLocked());
	ui->txtTimeStamp->setVisible(ui->rdTimeStampCustom->isChecked());
	ui->txtTimeStamp->setPlaceholderText(Application::confValue(Settings::TSA_URL.KEY).toString());
	ui->txtTimeStamp->setText(Settings::TSA_URL);
	ui->wgtTSACert->setDisabled(Settings::TSA_CERT.isLocked());
	ui->wgtTSACert->setVisible(ui->rdTimeStampCustom->isChecked());
	connect(ui->rdTimeStampCustom, &QRadioButton::toggled, ui->txtTimeStamp, [this](bool checked) {
		ui->txtTimeStamp->setVisible(checked);
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
	ui->txtMIDUUID->setVisible(ui->rdMIDUUIDCustom->isChecked());
	ui->txtMIDUUID->setText(Settings::MID_UUID);
	connect(ui->rdMIDUUIDCustom, &QRadioButton::toggled, ui->txtMIDUUID, [this](bool checked) {
		ui->txtMIDUUID->setVisible(checked);
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

	// pageValidation
	ui->rdSiVaDefault->setDisabled(Settings::SIVA_URL_CUSTOM.isLocked());
	ui->rdSiVaCustom->setEnabled(ui->rdSiVaDefault->isEnabled());
	ui->rdSiVaCustom->setChecked(Settings::SIVA_URL_CUSTOM);
	ui->txtSiVa->setReadOnly(Settings::SIVA_URL.isLocked());
	ui->txtSiVa->setVisible(ui->rdSiVaCustom->isChecked());
	ui->txtSiVa->setPlaceholderText(Application::confValue(Settings::SIVA_URL.KEY).toString());
	ui->txtSiVa->setText(Settings::SIVA_URL);
	ui->wgtSiVaCert->setDisabled(Settings::SIVA_CERT.isLocked());
	ui->wgtSiVaCert->setVisible(ui->rdSiVaCustom->isChecked());
	connect(ui->rdSiVaCustom, &QRadioButton::toggled, ui->txtSiVa, [this](bool checked) {
		ui->txtSiVa->setVisible(checked);
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

	// pageEncryption
	ui->wgtCDoc2->hide();
	connect(ui->rdCdoc2, &QRadioButton::toggled, ui->wgtCDoc2, &QWidget::setVisible);
	ui->rdCdoc2->setChecked(Settings::CDOC2_DEFAULT);
	connect(ui->rdCdoc2, &QRadioButton::toggled, this, Settings::CDOC2_DEFAULT);
	ui->wgtCDoc2Server->hide();
	connect(ui->chkCdoc2KeyServer, &QCheckBox::toggled, ui->wgtCDoc2Server, &QWidget::setVisible);
	ui->chkCdoc2KeyServer->setChecked(Settings::CDOC2_USE_KEYSERVER);
	connect(ui->chkCdoc2KeyServer, &QCheckBox::toggled, this, Settings::CDOC2_USE_KEYSERVER);
#ifdef CONFIG_URL
	QJsonObject list = Application::confValue(QLatin1String("CDOC2-CONF")).toObject();
	auto setCDoc2Values = [this, list](const QString &key) {
		ui->txtCdoc2UUID->setText(key);
		QJsonObject data = list.value(key).toObject();
		ui->txtCdoc2Fetch->setText(data.value(QLatin1String("FETCH")).toString(Settings::CDOC2_GET));
		ui->txtCdoc2Post->setText(data.value(QLatin1String("POST")).toString(Settings::CDOC2_POST));
		bool disabled = ui->cmbCdoc2Name->currentIndex() < ui->cmbCdoc2Name->count() - 1;
		ui->txtCdoc2UUID->setDisabled(disabled);
		ui->txtCdoc2Fetch->setDisabled(disabled);
		ui->txtCdoc2Post->setDisabled(disabled);
		ui->txtCdoc2UUID->setClearButtonEnabled(!disabled);
		ui->txtCdoc2Fetch->setClearButtonEnabled(!disabled);
		ui->txtCdoc2Post->setClearButtonEnabled(!disabled);
		ui->wgtCDoc2Cert->setHidden(disabled);
	};
	for(QJsonObject::const_iterator i = list.constBegin(); i != list.constEnd(); ++i)
		ui->cmbCdoc2Name->addItem(i.value().toObject().value(QLatin1String("NAME")).toString(), i.key());
	if(QUuid(QString(Settings::CDOC2_UUID)).isNull())
		Settings::CDOC2_UUID = QUuid::createUuid().toString(QUuid::WithoutBraces);
	ui->cmbCdoc2Name->addItem(tr("Use a manually specified key transfer server for encryption"), Settings::CDOC2_UUID);
	QString cdoc2Service = Settings::CDOC2_DEFAULT_KEYSERVER;
	ui->cmbCdoc2Name->setCurrentIndex(ui->cmbCdoc2Name->findData(cdoc2Service));
	connect(ui->cmbCdoc2Name, qOverload<int>(&QComboBox::currentIndexChanged), this, [this, setCDoc2Values] (int index) {
		QString key = ui->cmbCdoc2Name->itemData(index).toString();
		Settings::CDOC2_DEFAULT_KEYSERVER = key;
		setCDoc2Values(key);
	});
	setCDoc2Values(cdoc2Service);
	connect(ui->txtCdoc2UUID, &QLineEdit::textEdited, this, [](const QString &uuid) {
		Settings::CDOC2_UUID = uuid;
		Settings::CDOC2_DEFAULT_KEYSERVER = uuid;
	});
	connect(ui->txtCdoc2Fetch, &QLineEdit::textEdited, this, [this](const QString &url) {
		Settings::CDOC2_GET = url;
		if(url.isEmpty())
		{
			Settings::CDOC2_GET_CERT.clear();
			Settings::CDOC2_POST_CERT.clear();
			updateCDoc2Cert(QSslCertificate());
		}
	});
	connect(ui->txtCdoc2Post, &QLineEdit::textEdited, this, [this](const QString &url) {
		Settings::CDOC2_POST = url;
		if(url.isEmpty())
		{
			Settings::CDOC2_GET_CERT.clear();
			Settings::CDOC2_POST_CERT.clear();
			updateCDoc2Cert(QSslCertificate());
		}
	});
#else
	ui->cmbCdoc2Name->addItem(QStringLiteral("Default"));
	ui->txtCdoc2UUID->setText(QStringLiteral("00000000-0000-0000-0000-000000000000"));
	ui->txtCdoc2Fetch->setText(QStringLiteral(CDOC2_GET_URL));
	ui->txtCdoc2Post->setText(QStringLiteral(CDOC2_POST_URL));
#endif
	connect(ui->btInstallCDoc2Cert, &QPushButton::clicked, this, [this] {
		QSslCertificate cert = selectCert(tr("Select a key transfer server certificate"),
			tr("Key transfer server SSL certificate"));
		if(cert.isNull())
			return;
		Settings::CDOC2_GET_CERT = cert.toDer().toBase64();
		Settings::CDOC2_POST_CERT = cert.toDer().toBase64();
		updateCDoc2Cert(cert);
	});
	updateCDoc2Cert(QSslCertificate(QByteArray::fromBase64(Settings::CDOC2_GET_CERT), QSsl::Der));

	// pageProxy
	connect(this, &SettingsDialog::finished, this, &SettingsDialog::saveProxy);
	ui->proxyGroup->setId(ui->rdProxyNone, Settings::ProxyNone);
	ui->proxyGroup->setId(ui->rdProxySystem, Settings::ProxySystem);
	ui->proxyGroup->setId(ui->rdProxyManual, Settings::ProxyManual);
	ui->wgtProxyManual->hide();
	connect(ui->rdProxyManual, &QRadioButton::toggled, ui->wgtProxyManual, &QWidget::setVisible);
	ui->proxyGroup->button(Settings::PROXY_CONFIG)->setChecked(true);
#ifdef Q_OS_MACOS
	ui->txtProxyHost->setText(Settings::PROXY_HOST);
	ui->txtProxyPort->setText(Settings::PROXY_PORT);
	ui->txtProxyUsername->setText(Settings::PROXY_USER);
	ui->txtProxyPassword->setText(Settings::PROXY_PASS);
	connect(ui->txtProxyHost, &QLineEdit::textChanged, this, Settings::PROXY_HOST);
	connect(ui->txtProxyPort, &QLineEdit::textChanged, this, Settings::PROXY_PORT);
	connect(ui->txtProxyUsername, &QLineEdit::textChanged, this, Settings::PROXY_USER);
	connect(ui->txtProxyPassword, &QLineEdit::textChanged, this, Settings::PROXY_PASS);
#else
	if(auto *i = digidoc::XmlConfCurrent::instance())
	{
		ui->txtProxyHost->setText(QString::fromStdString(i->digidoc::XmlConfCurrent::proxyHost()));
		ui->txtProxyPort->setText(QString::fromStdString(i->digidoc::XmlConfCurrent::proxyPort()));
		ui->txtProxyUsername->setText(QString::fromStdString(i->digidoc::XmlConfCurrent::proxyUser()));
		ui->txtProxyPassword->setText(QString::fromStdString(i->digidoc::XmlConfCurrent::proxyPass()));
	}
#endif

	// pageDiagnostics
	ui->structureFunds->load(QStringLiteral(":/images/Struktuurifondid.svg"));
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
#ifdef Q_OS_MACOS
		WarningDialog::show(this, tr("Restart DigiDoc4 Client to activate logging. Read more "
			"<a href=\"https://www.id.ee/en/article/log-file-generation-in-digidoc4-client/\">here</a>."));
#else
		auto *dlg = WarningDialog::show(this, tr("Restart DigiDoc4 Client to activate logging. Read more "
			"<a href=\"https://www.id.ee/en/article/log-file-generation-in-digidoc4-client/\">here</a>. Restart now?"));
		dlg->setCancelText(WarningDialog::NO);
		dlg->addButton(WarningDialog::YES, QMessageBox::Yes);
		connect(dlg, &WarningDialog::finished, qApp, [](int result) {
			if(result == QMessageBox::Yes) {
				qApp->setProperty("restart", true);
				QApplication::quit();
			}
		});
#endif
	});

	// navigationArea
	connect(ui->btNavClose, &QPushButton::clicked, this, &SettingsDialog::accept);
	connect(this, &SettingsDialog::finished, this, &SettingsDialog::close);

	connect(ui->btnCheckConnection, &QPushButton::clicked, this, &SettingsDialog::checkConnection);
	connect(ui->btnFirstRun, &QPushButton::clicked, this, [this] {
		auto *dlg = new FirstRun(this);
		connect(dlg, &FirstRun::langChanged, this, [this](const QString &lang) {
			retranslate(lang);
			selectLanguage();
		});
		dlg->open();
	});
#ifdef CONFIG_URL
	connect(qApp->conf(), &Configuration::finished, this, [this](bool /*update*/, const QString &error){
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
	connect(ui->btnRefreshConfig, &QPushButton::clicked, this, [] {
#ifdef CONFIG_URL
		qApp->conf()->update(true);
#endif
		Application::updateTSLCache({});
	});
	connect(ui->btnNavUseByDefault, &QPushButton::clicked, this, &SettingsDialog::useDefaultSettings);
	connect(ui->btnNavSaveReport, &QPushButton::clicked, this, [this]{
		saveFile(QStringLiteral("diagnostics.txt"), ui->txtDiagnostics->toPlainText().toUtf8());
	});
	connect(ui->btnNavSaveLibdigidocpp, &QPushButton::clicked, this, [this]{
		Settings::LIBDIGIDOCPP_DEBUG = false;
		QString log = QStringLiteral("%1/libdigidocpp.log").arg(QDir::tempPath());
		saveFile(QStringLiteral("libdigidocpp.txt"), log);
		saveFile(QStringLiteral("qdigidoc4.txt"), qdigidoc4log);
		QFile::remove(log);
		ui->btnNavSaveLibdigidocpp->hide();
	});
#ifdef Q_OS_WIN
	connect(ui->btnNavFromHistory, &QPushButton::clicked, this, [this] {
		// remove certificates from browsing history of Edge and Google Chrome, and do it for all users.
		QList<TokenData> cache = qApp->signer()->cache();
		HCERTSTORE s = CertOpenStore(CERT_STORE_PROV_SYSTEM_W,
			X509_ASN_ENCODING, 0, CERT_SYSTEM_STORE_CURRENT_USER, L"MY");
		if(!s)
			return;

		auto scope = qScopeGuard([&s] {
			CertCloseStore(s, 0);
		});

		PCCERT_CONTEXT c{};
		while((c = CertEnumCertificatesInStore(s, c)))
		{
			QSslCertificate cert(QByteArray::fromRawData((char*)c->pbCertEncoded, c->cbCertEncoded), QSsl::Der);
			if(std::any_of(cache.cbegin(), cache.cend(), [&](const TokenData &token) { return token.cert() == cert; }))
				continue;
			if(cert.issuerInfo(QSslCertificate::CommonName).join(QString()).contains(u"KLASS3-SK"_s, Qt::CaseInsensitive) ||
				cert.issuerInfo(QSslCertificate::Organization).contains(u"SK ID Solutions AS"_s, Qt::CaseInsensitive) ||
				cert.issuerInfo(QSslCertificate::Organization).contains(u"Zetes Estonia OÜ"_s, Qt::CaseInsensitive))
				CertDeleteCertificateFromStore(CertDuplicateCertificateContext(c));
		}
		WarningDialog::show(this, tr("Redundant certificates have been successfully removed."));
	});
#endif

	ui->pageGroup->setId(ui->btnMenuGeneral, GeneralSettings);
	ui->pageGroup->setId(ui->btnMenuCertificate, SigningSettings);
	ui->pageGroup->setId(ui->btnMenuValidation, ValidationSettings);
	ui->pageGroup->setId(ui->btnMenuEncryption, EncryptionSettings);
	ui->pageGroup->setId(ui->btnMenuProxy, NetworkSettings);
	ui->pageGroup->setId(ui->btnMenuDiagnostics, DiagnosticsSettings);
	ui->pageGroup->setId(ui->btnMenuInfo, LicenseSettings);
	connect(ui->pageGroup, &QButtonGroup::idClicked, this, &SettingsDialog::showPage);

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
		QString error;
		QString details = connection.errorDetails();
		QTextStream s(&error);
		s << connection.errorString();
		if (!details.isEmpty())
			s << "\n\n" << details << ".";
		FadeInNotification::error(this, error, 120);
	}
	else
	{
		Application::restoreOverrideCursor();
		FadeInNotification::success(this, tr("The connection to certificate status service is successful!"));
	}
}

void SettingsDialog::retranslate(const QString& lang)
{
	qApp->loadTranslation( lang );
	ui->retranslateUi(this);
	updateDiagnostics();
	ui->cmbCdoc2Name->setItemText(ui->cmbCdoc2Name->count() - 1,
		tr("Use a manually specified key transfer server for encryption"));
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

void SettingsDialog::updateCDoc2Cert(const QSslCertificate &c)
{
	updateCert(c, ui->btShowCDoc2Cert, ui->txtCDoc2Cert);
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

void SettingsDialog::saveProxy()
{
	Settings::PROXY_CONFIG = ui->proxyGroup->checkedId();
#ifndef Q_OS_MACOS
	if(auto *i = digidoc::XmlConfCurrent::instance())
	{
		i->setProxyHost(ui->txtProxyHost->text().toStdString());
		i->setProxyPort(ui->txtProxyPort->text().toStdString());
		i->setProxyUser(ui->txtProxyUsername->text().toStdString());
		i->setProxyPass(ui->txtProxyPassword->text().toStdString());
	}
#endif
	loadProxy(digidoc::Conf::instance());
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
	ui->txtNavVersion->setText(tr("DigiDoc4 version %1, released %2")
		.arg(QApplication::applicationVersion(), QStringLiteral(BUILD_DATE)));
	ui->txtDiagnostics->setEnabled(false);
	ui->txtDiagnostics->clear();
	ui->btnNavSaveReport->setDisabled(true);

	QApplication::setOverrideCursor( Qt::WaitCursor );
	auto *worker = new Diagnostics();
	connect(worker, &Diagnostics::update, ui->txtDiagnostics, &QTextBrowser::insertHtml, Qt::QueuedConnection);
	connect(worker, &Diagnostics::destroyed, this, [this]{
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
	ui->rdCdoc2->setChecked(Settings::CDOC2_DEFAULT.DEFAULT);
}

void SettingsDialog::showPage(int page)
{
	ui->pageGroup->button(page)->setChecked(true);
	ui->stackedWidget->setCurrentIndex(page);
	ui->btnNavUseByDefault->setVisible(page == SigningSettings || page == ValidationSettings);
	ui->btnFirstRun->setVisible(page == GeneralSettings);
	ui->btnRefreshConfig->setVisible(page == GeneralSettings);
	ui->btnCheckConnection->setVisible(page == NetworkSettings);
	ui->btnNavSaveReport->setVisible(page == DiagnosticsSettings);
	ui->btnNavSaveLibdigidocpp->setVisible(page == DiagnosticsSettings &&
		QFile::exists(QStringLiteral("%1/libdigidocpp.log").arg(QDir::tempPath())));
#ifdef Q_OS_WIN
	ui->btnNavFromHistory->setVisible(page == GeneralSettings);
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
