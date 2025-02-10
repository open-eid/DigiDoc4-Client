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

#define NOMINMAX

#include "Application.h"

#include "Common.h"
#include "Configuration.h"
#include "MainWindow.h"
#include "QSigner.h"
#include "QSmartCard.h"
#include "DigiDoc.h"
#include "Settings.h"
#include "Styles.h"
#ifdef Q_OS_MAC
#include "MacMenuBar.h"
#else
class MacMenuBar {};
#endif
#include "TokenData.h"
#include "Utils.h"
#include "dialogs/FirstRun.h"
#include "dialogs/SettingsDialog.h"
#include "dialogs/WaitDialog.h"
#include "dialogs/WarningDialog.h"
#include "effects/Overlay.h"

#include <optional>

#include <digidocpp/Container.h>
#include <digidocpp/XmlConf.h>
#include <digidocpp/crypto/X509Cert.h>

#include <qtsingleapplication/src/qtlocalpeer.h>

#include <QAction>
#include <QtCore/QFileInfo>
#include <QtCore/QJsonArray>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <QtCore/QProcess>
#include <QtCore/QRegularExpression>
#include <QtCore/QStandardPaths>
#include <QtCore/QTimer>
#include <QtCore/QTranslator>
#include <QtCore/QUrl>
#include <QtCore/QUrlQuery>
#include <QtCore/QVersionNumber>
#include <QtCore/QXmlStreamReader>
#include <QtGui/QDesktopServices>
#include <QtGui/QFileOpenEvent>
#include <QtGui/QScreen>
#include <QtNetwork/QNetworkProxy>
#include <QtNetwork/QSslCertificate>
#include <QtNetwork/QSslConfiguration>
#include <QtWidgets/QAccessibleWidget>
#include <QtWidgets/QMessageBox>
#include <QtWidgets/QProgressBar>
#include <QtWidgets/QProgressDialog>
#include <QtWidgets/QToolTip>

#ifdef Q_OS_WIN32
#include <QtCore/QLibrary>
#include <qt_windows.h>
#include <MAPI.h>
#endif

using namespace std::chrono;

const QStringList Application::CONTAINER_EXT {
	QStringLiteral("asice"), QStringLiteral("sce"),
	QStringLiteral("asics"), QStringLiteral("scs"),
	QStringLiteral("bdoc"), QStringLiteral("edoc"),
};

class DigidocConf final: public digidoc::XmlConfCurrent
{
public:
	DigidocConf()
	{
		enableLog(Settings::LIBDIGIDOCPP_DEBUG);
		Settings::LIBDIGIDOCPP_DEBUG = false;
		Settings::LIBDIGIDOCPP_DEBUG.registerCallback([this](const bool &value) { enableLog(value); });
#ifndef Q_OS_DARWIN
		setTSLOnlineDigest(true);
#endif
#ifdef CONFIG_URL
		reload();
		Configuration::connect(qApp->conf(), &Configuration::finished, [](bool changed, const QString & /*error*/){
			if(changed)
				reload();
		});
#endif
		SettingsDialog::loadProxy(this);
	}

	int logLevel() const final
	{
		return log.has_value() ? 4 : digidoc::XmlConfCurrent::logLevel();
	}

	std::string logFile() const final
	{
		return log.value_or(digidoc::XmlConfCurrent::logFile());
	}

	std::string proxyHost() const final
	{
		return proxyConf(&QNetworkProxy::hostName,
			Settings::PROXY_HOST, [this] { return digidoc::XmlConfCurrent::proxyHost(); });
	}

	std::string proxyPort() const final
	{
		return proxyConf([](const QNetworkProxy &systemProxy) { return QString::number(systemProxy.port()); },
			Settings::PROXY_PORT, [this] { return digidoc::XmlConfCurrent::proxyPort(); });
	}

	std::string proxyUser() const final
	{
		return proxyConf(&QNetworkProxy::user,
			Settings::PROXY_USER, [this] { return digidoc::XmlConfCurrent::proxyUser(); });
	}

	std::string proxyPass() const final
	{
		return proxyConf(&QNetworkProxy::password,
			Settings::PROXY_PASS, [this] { return digidoc::XmlConfCurrent::proxyPass(); });
	}

#ifdef Q_OS_MAC
	std::string TSLCache() const final
	{ return QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation).toStdString(); }
#endif

	std::vector<digidoc::X509Cert> TSCerts() const final
	{
		std::vector<digidoc::X509Cert> list = toCerts(QLatin1String("CERT-BUNDLE"));
		if(digidoc::X509Cert cert = toCert(fromBase64(Settings::TSA_CERT)))
			list.push_back(std::move(cert));
		list.emplace_back(); // Make sure that TSA cert pinning is enabled
		return list;
	}

	std::string TSUrl() const final
	{
		return valueUserScope(Settings::TSA_URL_CUSTOM, Settings::TSA_URL, digidoc::XmlConfCurrent::TSUrl());
	}

	std::string TSLUrl() const final
	{ return valueSystemScope(QLatin1String("TSL-URL"), digidoc::XmlConfCurrent::TSLUrl()); }
	std::vector<digidoc::X509Cert> TSLCerts() const final
	{
		std::vector<digidoc::X509Cert> tslcerts = toCerts(QLatin1String("TSL-CERTS"));
		return tslcerts.empty() ? digidoc::XmlConfCurrent::TSLCerts() : std::move(tslcerts);
	}

	digidoc::X509Cert verifyServiceCert() const final
	{
		QByteArray cert = fromBase64(Settings::SIVA_CERT);
		return cert.isEmpty() ? digidoc::XmlConfCurrent::verifyServiceCert() : toCert(cert);
	}
	std::vector<digidoc::X509Cert> verifyServiceCerts() const final
	{
		std::vector<digidoc::X509Cert> list = toCerts(QLatin1String("CERT-BUNDLE"));
		if(digidoc::X509Cert cert = verifyServiceCert())
			list.push_back(std::move(cert));
		list.emplace_back(); // Make sure that TSA cert pinning is enabled
		return list;
	}
	std::string verifyServiceUri() const final
	{
		return valueUserScope(Settings::SIVA_URL_CUSTOM, Settings::SIVA_URL, digidoc::XmlConfCurrent::verifyServiceUri());
	}

	bool TSLAllowExpired() const final
	{
		if(static std::atomic_bool isShown(false); !isShown.exchange(true))
		{
			dispatchToMain([] {
				WarningDialog::show(Application::tr(
					"The renewal of Trust Service status List, used for digital signature validation, has failed. "
					"Please check your internet connection and make sure you have the latest ID-software version "
					"installed. An expired Trust Service List (TSL) will be used for signature validation. "
					"<a href=\"https://www.id.ee/en/article/digidoc4-message-updating-the-list-of-trusted-certificates-was-unsuccessful/\">Additional information</a>"), QString());
			});
		}
		return true;
	}

private:
#ifdef CONFIG_URL
	static void reload()
	{
		if(Settings::TSA_URL == Application::confValue(Settings::TSA_URL.KEY).toString())
			Settings::TSA_URL.clear(); // Cleanup user conf if it is default url
	}
#endif

	void enableLog(bool enable)
	{
		if(enable)
			log = QStringLiteral("%1/libdigidocpp.log").arg(QDir::tempPath()).toStdString();
		else
			log.reset();
	}

	template<class T>
	static std::string valueSystemScope(const T &key, std::string &&defaultValue)
	{
		if(auto value = Application::confValue(key); value.isString())
			return value.toString().toStdString();
		return std::move(defaultValue);
	}

	template<typename Option>
	static std::string valueUserScope(bool custom, const Option &option, std::string &&defaultValue)
	{
		return custom && option.isSet() ? option : valueSystemScope(option.KEY, std::move(defaultValue));
	}

	template<typename System, typename Config, class Option>
	static std::string proxyConf(System &&system, const Option &option, Config &&config)
	{
		switch(Settings::PROXY_CONFIG)
		{
		case Settings::ProxyNone: return {};
		case Settings::ProxySystem: return std::invoke(system, [] {
				for(const QNetworkProxy &proxy: QNetworkProxyFactory::systemProxyForQuery())
				{
					if(proxy.type() == QNetworkProxy::HttpProxy)
						return proxy;
				}
				return QNetworkProxy{};
			}()).toStdString();
		default: return option.isSet() ? option : config();
		}
	}

	template<class T>
	static QByteArray fromBase64(const T &data)
	{
		if constexpr (std::is_convertible_v<T, QByteArray>)
			return QByteArray::fromBase64(data);
		else
			return QByteArray::fromBase64(data.toString().toLatin1());
	}

	static digidoc::X509Cert toCert(const QByteArray &der)
	{
		return digidoc::X509Cert((const unsigned char*)der.constData(), size_t(der.size()));
	}

	static std::vector<digidoc::X509Cert> toCerts(QLatin1String key)
	{
		std::vector<digidoc::X509Cert> certs;
		for(const auto list = Application::confValue(key).toArray(); auto cert: list)
		{
			if(QByteArray der = fromBase64(cert); !der.isEmpty())
				certs.emplace_back((const unsigned char*)der.constData(), size_t(der.size()));
		}
		return certs;
	}

	std::optional<std::string> log;
};

class Application::Private
{
public:
	Configuration *conf {};
	QAction		*closeAction {}, *newClientAction {}, *helpAction {};
	std::unique_ptr<MacMenuBar> bar;
	QSigner		*signer {};

	QTranslator	appTranslator, commonTranslator, qtTranslator;
	QString		lang;
	QTimer		lastWindowTimer;
	volatile bool ready = false;
#ifdef Q_OS_WIN
	QStringList	tempFiles;
#endif // Q_OS_WIN
};

Application::Application( int &argc, char **argv )
	: BaseApplication(argc, argv)
	, d(new Private)
{
	setApplicationName(QStringLiteral("qdigidoc4"));
	setApplicationVersion(QStringLiteral(VERSION_STR));
	setOrganizationDomain(QStringLiteral("ria.ee"));
	setOrganizationName(QStringLiteral("RIA"));
	setWindowIcon(QIcon(QStringLiteral(":/images/Icon.svg")));
	if(QFile::exists(QStringLiteral("%1/%2.log").arg(QDir::tempPath(), applicationName())))
		qInstallMessageHandler(msgHandler);

#if defined(Q_OS_WIN)
	AllowSetForegroundWindow( ASFW_ANY );
#ifdef NDEBUG
	setLibraryPaths({ applicationDirPath() });
#endif
#elif defined(Q_OS_MAC)
	qputenv("OPENSSL_CONF", applicationDirPath().toUtf8() + "../Resources/openssl.cnf");
#ifdef NDEBUG
	setLibraryPaths({ applicationDirPath() + "/../PlugIns" });
#endif
#endif
	setStyleSheet(QStringLiteral(
		"QDialogButtonBox { dialogbuttonbox-buttons-have-icons: 0; }\n"));

	QNetworkProxyFactory::setUseSystemConfiguration(true);

	QStringList args = arguments();
	args.removeFirst();
#ifndef Q_OS_MAC
	if( isRunning() )
	{
		sendMessage(args.join(QStringLiteral("\", \"")));
		return;
	}
	connect(this, &Application::messageReceived, this, qOverload<const QString&>(&Application::parseArgs));
#endif

#ifdef CONFIG_URL
	d->conf = new Configuration(this);
	QMetaObject::invokeMethod(this, [this] {
		auto lessThanVersion = [](QLatin1String key) {
			return QVersionNumber::fromString(applicationVersion()) <
				QVersionNumber::fromString(confValue(key).toString());
		};
		WarningDialog *dlg{};
		if(lessThanVersion(QLatin1String("QDIGIDOC4-UNSUPPORTED")))
		{
			dlg = WarningDialog::show(tr(
				"This version of ID-software on your computer is unsupported. "
				"DigiDoc4 Client cannot be used until you update ID-software. "
				"Install new ID-software from <a href=\"https://www.id.ee/en/article/install-id-software/\">www.id.ee</a>. "
				"macOS users can download the latest ID-software version from the "
				"<a href=\"https://itunes.apple.com/ee/developer/ria/id556524921?mt=12\">Mac App Store</a>."));
			connect(dlg, &WarningDialog::finished, this, &Application::quit);
		}
		else if(lessThanVersion(QLatin1String("QDIGIDOC4-SUPPORTED")))
		{
			dlg = WarningDialog::show(tr(
				"Your ID-software has expired. To download the latest software version, go to the "
				"<a href=\"https://www.id.ee/en/article/install-id-software/\">id.ee</a> website. "
				"macOS users can download the latest ID-software version from the "
				"<a href=\"https://itunes.apple.com/ee/developer/ria/id556524921?mt=12\">Mac App Store</a>."));
		}
		connect(d->conf, &Configuration::finished, this, [lessThanVersion](bool changed, const QString &){
			if(changed && lessThanVersion(QLatin1String("QDIGIDOC4-LATEST")))
			{
				auto *dlg = new WarningDialog(tr(
					"An ID-software update has been found. To download the update, go to the "
					"<a href=\"https://www.id.ee/en/article/install-id-software/\">id.ee</a> website. "
					"macOS users can download the update from the "
					"<a href=\"https://itunes.apple.com/ee/developer/ria/id556524921?mt=12\">Mac App Store</a>."), activeWindow());
				new Overlay(dlg, activeWindow());
				dlg->exec();
			}
		});
#ifdef Q_OS_WIN
		if(dlg)
		{
			dlg->addButton(tr("Start downloading"), 2);
			connect(dlg, &WarningDialog::finished, this, [](int result) {
				if(result != 2)
					return;
				QString path = QApplication::applicationDirPath() + QLatin1String("/id-updater.exe");
				if (QFile::exists(path))
					QProcess::startDetached(path, {});
			});
		}
#endif
	}, Qt::QueuedConnection);
#endif

	qRegisterMetaType<TokenData>("TokenData");
	qRegisterMetaType<QSmartCardData>("QSmartCardData");
	qRegisterMetaType<QEventLoop*>("QEventLoop*");
	QToolTip::setFont(Styles::font(Styles::Regular, 14));
	QDesktopServices::setUrlHandler(QStringLiteral("browse"), this, "browse");
	QDesktopServices::setUrlHandler(QStringLiteral("mailto"), this, "mailTo");
	QAccessible::installFactory([](const QString &classname, QObject *object) -> QAccessibleInterface* {
		if (classname == QLatin1String("QSvgWidget") && object && object->isWidgetType())
			return new QAccessibleWidget(qobject_cast<QWidget *>(object), QAccessible::StaticText);
		return {};
	});

	installTranslator( &d->appTranslator );
	installTranslator( &d->commonTranslator );
	installTranslator( &d->qtTranslator );
	loadTranslation(Settings::LANGUAGE);

	// Clear obsolete registriy settings
	Settings::SETTINGS_MIGRATED.clear();
#ifdef Q_OS_DARWIN
	Settings::TSL_ONLINE_DIGEST.clear();
#endif

	// Actions
	d->closeAction = new QAction( tr("Close Window"), this );
	d->closeAction->setShortcut( Qt::CTRL + Qt::Key_W );
	connect(d->closeAction, &QAction::triggered, this, &Application::closeWindow);
	d->newClientAction = new QAction( tr("New Window"), this );
	d->newClientAction->setShortcut( Qt::CTRL + Qt::Key_N );
	connect(d->newClientAction, &QAction::triggered, this, []{ showClient({}, false, false, true); });

	// This is needed to release application from memory (Windows)
	setQuitOnLastWindowClosed( true );
	d->lastWindowTimer.setSingleShot(true);
	connect(&d->lastWindowTimer, &QTimer::timeout, this, []{ if(topLevelWindows().isEmpty()) quit(); });
	connect(this, &Application::lastWindowClosed, this, [&]{ d->lastWindowTimer.start(10s); });

#ifdef Q_OS_MAC
	d->bar = std::make_unique<MacMenuBar>();
	d->bar->fileMenu()->addAction( d->newClientAction );
	d->bar->fileMenu()->addAction( d->closeAction );
	d->bar->dockMenu()->addAction( d->newClientAction );
	d->helpAction = d->bar->helpMenu()->addAction(tr("DigiDoc4 Client Help"), this, &Application::openHelp);
#endif

	try
	{
		digidoc::Conf::init( new DigidocConf );
		d->signer = new QSigner(this);
		updateTSLCache(QDateTime::currentDateTimeUtc().addDays(-7));

		digidoc::initialize(applicationName().toUtf8().constData(), QStringLiteral("%1/%2 (%3)")
			.arg(applicationName(), applicationVersion(), Common::applicationOs()).toUtf8().constData(),
			[](const digidoc::Exception *ex) {
				qDebug() << "TSL loading finished";
				Q_EMIT qApp->TSLLoadingFinished();
				qApp->d->ready = true;
				if(ex)
					dispatchToMain(showWarning, tr("Failed to initalize."), *ex);
			}
		);
	}
	catch( const digidoc::Exception &e )
	{
		showWarning( tr("Failed to initalize."), e );
		setQuitOnLastWindowClosed( true );
		return;
	}

	QMetaObject::invokeMethod(this, [this] {
#ifdef Q_OS_MAC
		if(!Settings::PLUGINS.isSet())
		{
			auto *dlg = new WarningDialog(tr(
				"In order to authenticate and sign in e-services with an ID-card you need to install the web browser components."), mainWindow());
			dlg->setCancelText(tr("Ignore forever").toUpper());
			dlg->addButton(tr("Remind later").toUpper(), QMessageBox::Ignore);
			dlg->addButton(tr("Install").toUpper(), QMessageBox::Open);
			connect(dlg, &WarningDialog::finished, this, [](int result) {
				switch(result)
				{
				case QMessageBox::Open: QDesktopServices::openUrl(tr("https://www.id.ee/en/article/install-id-software/")); break;
				case QMessageBox::Ignore: break;
				default: Settings::PLUGINS = QStringLiteral("ignore");
				}
			});
			dlg->open();
		}
#endif
		if(Settings::SHOW_INTRO)
		{
			Settings::SHOW_INTRO = false;
			auto *dlg = new FirstRun(mainWindow());
			connect(dlg, &FirstRun::langChanged, this, &Application::loadTranslation);
			dlg->open();
		}
	}, Qt::QueuedConnection);

	if( !args.isEmpty() || topLevelWindows().isEmpty() )
		parseArgs(std::move(args));
}

Application::~Application()
{
	for(QWidget *top: topLevelWidgets())
		top->close();
#ifdef Q_OS_WIN
	for(const QString &file: qAsConst(d->tempFiles))
		QFile::remove(file);
	d->tempFiles.clear();
#endif // Q_OS_WIN

#ifndef Q_OS_MAC
	if( isRunning() )
	{
		delete d;
		return;
	}
	if(auto *obj = findChild<QtLocalPeer*>())
		delete obj;
#else
	deinitMacEvents();
#endif
	QEventLoop e;
	connect(this, &Application::TSLLoadingFinished, &e, &QEventLoop::quit);
	if( !d->ready )
		e.exec();
	digidoc::terminate();
	delete d;

	QDesktopServices::unsetUrlHandler(QStringLiteral("browse"));
	QDesktopServices::unsetUrlHandler(QStringLiteral("mailto"));

	if(property("restart").toBool())
	{
		QStringList args = arguments();
		args.removeFirst();
		QProcess::startDetached(applicationFilePath(), args);
	}
}

#ifndef Q_OS_MAC
void Application::addRecent( const QString & ) {}
#endif

#ifdef Q_OS_WIN
void Application::addTempFile(const QString &file)
{
	d->tempFiles.append(file);
}
#endif

void Application::browse( const QUrl &url )
{
	QUrl u = url;
	u.setScheme(QStringLiteral("file"));
#if defined(Q_OS_WIN)
	if(QProcess::startDetached(QStringLiteral("explorer"), {QStringLiteral("/select,"), QDir::toNativeSeparators(u.toLocalFile())}))
		return;
#elif defined(Q_OS_MAC)
	if(QProcess::startDetached(QStringLiteral("open"), {QStringLiteral("-R"), u.toLocalFile()}))
		return;
#endif
	QDesktopServices::openUrl( QUrl::fromLocalFile( QFileInfo( u.toLocalFile() ).absolutePath() ) );
}

void Application::closeWindow()
{
#ifndef Q_OS_MAC
	if(auto *w = qobject_cast<MainWindow*>(activeWindow()))
		w->close();
	else
#endif
	if(auto *d = qobject_cast<QDialog*>(activeWindow()))
		d->reject();
	else if(QWidget *w = activeWindow())
		w->close();
}

Configuration* Application::conf()
{
	return d->conf;
}

template<class T>
QJsonValue Application::confValue(const T &key)
{
#ifdef CONFIG_URL
	return qApp->conf()->object().value(key);
#else
	Q_UNUSED(key)
	return {};
#endif
}

template QJsonValue Application::confValue<QString>(const QString &key);
template QJsonValue Application::confValue<QLatin1String>(const QLatin1String &key);

QVariant Application::confValue( ConfParameter parameter, const QVariant &value )
{
	auto *i = static_cast<DigidocConf*>(digidoc::Conf::instance());

	QByteArray r;
	switch( parameter )
	{
	case SiVaUrl: r = i->verifyServiceUri().c_str(); break;
	case TSAUrl: r = i->TSUrl().c_str(); break;
	case TSLUrl: r = i->TSLUrl().c_str(); break;
	case TSLCache: r = i->TSLCache().c_str(); break;
	case TSLCerts:
	{
		QList<QSslCertificate> list;
		for(const digidoc::X509Cert &cert: i->TSLCerts())
		{
			if(std::vector<unsigned char> v = cert; !v.empty())
				list.append(QSslCertificate(QByteArray::fromRawData((const char*)v.data(), int(v.size())), QSsl::Der));
		}
		return QVariant::fromValue(list);
	}
	}
	return r.isEmpty() ? value.toString() : QString::fromUtf8( r );
}

bool Application::event(QEvent *event)
{
	switch(int(event->type()))
	{
	case REOpenEvent::Type:
		if( !activeWindow() )
			parseArgs();
		return true;
	case QEvent::FileOpen:
	{
		QString fileName = static_cast<QFileOpenEvent*>(event)->file().normalized(QString::NormalizationForm_C);
		QMetaObject::invokeMethod(this, [fileName = std::move(fileName)] {
			parseArgs({ fileName });
		});
		return true;
	}
#ifdef Q_OS_MAC
	// Load here because cocoa NSApplication overides events
	case QEvent::ApplicationActivate:
		initMacEvents();
		return BaseApplication::event(event);
#endif
	default: return BaseApplication::event(event);
	}
}

void Application::initDiagnosticConf()
{
	digidoc::Conf::init(new DigidocConf);
}

void Application::loadTranslation( const QString &lang )
{
	if( d->lang == lang )
		return;
	Settings::LANGUAGE = d->lang = lang;

	if(lang == QLatin1String("en")) QLocale::setDefault(QLocale(QLocale::English, QLocale::UnitedKingdom));
	else if(lang == QLatin1String("ru")) QLocale::setDefault(QLocale( QLocale::Russian, QLocale::RussianFederation));
	else QLocale::setDefault(QLocale(QLocale::Estonian, QLocale::Estonia));

	void(d->appTranslator.load(QLatin1String(":/translations/%1.qm").arg(lang)));
	void(d->commonTranslator.load(QLatin1String(":/translations/common_%1.qm").arg(lang)));
	void(d->qtTranslator.load(QLatin1String(":/translations/qt_%1.qm").arg(lang)));
	if( d->closeAction ) d->closeAction->setText( tr("Close Window") );
	if( d->newClientAction ) d->newClientAction->setText( tr("New Window") );
	if(d->helpAction) d->helpAction->setText(tr("DigiDoc4 Client Help"));
}

#ifndef Q_OS_MAC
void Application::mailTo( const QUrl &url )
{
	QUrlQuery q(url);
#if defined(Q_OS_WIN)
	if(QLibrary lib("mapi32"); auto mapi = LPMAPISENDMAILW(lib.resolve("MAPISendMailW")))
	{
		QString file = q.queryItemValue( "attachment", QUrl::FullyDecoded );
		QString filePath = QDir::toNativeSeparators( file );
		QString fileName = QFileInfo( file ).fileName();
		QString subject = q.queryItemValue( "subject", QUrl::FullyDecoded );
		MapiFileDescW doc {};
		doc.nPosition = -1;
		doc.lpszPathName = PWSTR(filePath.utf16());
		doc.lpszFileName = PWSTR(fileName.utf16());
		MapiMessageW message {};
		message.lpszSubject = PWSTR(subject.utf16());
		message.lpszNoteText = PWSTR(L"");
		message.nFileCount = 1;
		message.lpFiles = lpMapiFileDescW(&doc);
		switch( mapi( NULL, 0, &message, MAPI_LOGON_UI|MAPI_DIALOG, 0 ) )
		{
		case SUCCESS_SUCCESS:
		case MAPI_E_USER_ABORT:
		case MAPI_E_LOGIN_FAILURE:
			return;
		default: break;
		}
	}
#elif defined(Q_OS_UNIX)
	QByteArray thunderbird;
	QProcess p;
	QStringList env = QProcess::systemEnvironment();
	if(env.indexOf(QRegularExpression(QStringLiteral("KDE_FULL_SESSION.*"))) != -1)
	{
		p.start("kreadconfig", {
			"--file", "emaildefaults",
			"--group", "PROFILE_Default",
			"--key", "EmailClient"});
		p.waitForFinished();
		if(QByteArray data = p.readAllStandardOutput().trimmed(); data.contains("thunderbird"))
			thunderbird = std::move(data);
	}
	else if(env.indexOf(QRegularExpression(QStringLiteral("GNOME_DESKTOP_SESSION_ID.*"))) != -1)
	{
		if(QSettings(QDir::homePath() + "/.local/share/applications/mimeapps.list", QSettings::IniFormat)
				.value("Default Applications/x-scheme-handler/mailto").toString().contains("thunderbird"))
			thunderbird = "/usr/bin/thunderbird";
		else
		{
			for(const QString &path: QProcessEnvironment::systemEnvironment().value("XDG_DATA_DIRS").split(":"))
			{
				if(QSettings(path + "/applications/defaults.list", QSettings::IniFormat)
						.value("Default Applications/x-scheme-handler/mailto").toString().contains("thunderbird"))
				{
					thunderbird = "/usr/bin/thunderbird";
					break;
				}
			}
		}
	}

	bool status = false;
	if( !thunderbird.isEmpty() )
	{
		status = p.startDetached(thunderbird, {"-compose",
			QStringLiteral("subject='%1',attachment='%2'")
				.arg( q.queryItemValue( "subject" ) )
				.arg(QUrl::fromLocalFile(q.queryItemValue("attachment")).toString())});
	}
	else
	{
		status = p.startDetached(QStringLiteral("xdg-email"), {
			"--subject", q.queryItemValue("subject"),
			"--attach", q.queryItemValue("attachment")});
	}
	if( status )
		return;
#endif
	QDesktopServices::openUrl( url );
}
#endif

QWidget* Application::mainWindow()
{
	if(auto *win = qobject_cast<MainWindow*>(activeWindow()))
		return win;
	auto list = topLevelWidgets();
	// Prefer main window; on Mac also the menu is top level window
	if(auto i = std::find_if(list.cbegin(), list.cend(),
			[](QWidget *widget) { return qobject_cast<MainWindow*>(widget); });
		i != list.cend())
		return *i;
	if(auto i = std::find_if(list.cbegin(), list.cend(),
			[](QWidget *widget) { return qobject_cast<QDialog*>(widget); });
		i != list.cend())
		return *i;
	return nullptr;
}

void Application::msgHandler(QtMsgType type, const QMessageLogContext &ctx, const QString &msg)
{
	QFile f(QStringLiteral("%1/%2.log").arg(QDir::tempPath(), applicationName()));
	if(!f.open( QFile::Append ))
		return;
	f.write(QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd hh:mm:ss ")).toUtf8());
	switch(type)
	{
	case QtDebugMsg: f.write("D"); break;
	case QtWarningMsg: f.write("W"); break;
	case QtCriticalMsg: f.write("C"); break;
	case QtFatalMsg: f.write("F"); break;
	default: f.write("I"); break;
	}
	f.write(QStringLiteral(" %1 ").arg(QLatin1String(ctx.category)).toUtf8());
	if(ctx.line > 0)
	{
		f.write(QStringLiteral("%1:%2 \"%3\" ")
					.arg(QFileInfo(QString::fromLatin1(ctx.file)).fileName())
					.arg(ctx.line)
					.arg(QLatin1String(ctx.function)).toUtf8());
	}
	f.write(msg.toUtf8());
	f.write("\n");
}

bool Application::notify(QObject *object, QEvent *event)
{
	try
	{
		return QApplication::notify(object, event);
	}
	catch( const digidoc::Exception &e )
	{
		showWarning( tr("Caught exception!"), e );
	}
	catch(const std::bad_alloc &e)
	{
		WarningDialog::show(tr("Added file(s) exceeds the maximum size limit of the container(120MB)."), QString::fromLocal8Bit(e.what()));
	}
	catch(...)
	{
		WarningDialog::show(tr("Caught exception!"));
	}

	return false;
}

void Application::openHelp()
{
	QDesktopServices::openUrl(QUrl(tr("https://www.id.ee/en/id-help/")));
}

void Application::parseArgs( const QString &msg )
{
	QStringList params;
	for(const QString &param: msg.split(QStringLiteral("\", \""), Qt::SkipEmptyParts))
	{
		QUrl url( param, QUrl::StrictMode );
		params.append(param != QLatin1String("-crypto") && !url.toLocalFile().isEmpty() ? url.toLocalFile() : param);
	}
	parseArgs(std::move(params));
}

void Application::parseArgs(QStringList args)
{
	bool crypto = args.removeAll(QStringLiteral("-crypto")) > 0;
	bool sign = args.removeAll(QStringLiteral("-sign")) > 0;
	bool newWindow = args.removeAll(QStringLiteral("-newWindow")) > 0;
	if(QString suffix = args.value(0);
		suffix.endsWith(QLatin1String(".cdoc"), Qt::CaseInsensitive) ||
		suffix.endsWith(QLatin1String(".cdoc2"), Qt::CaseInsensitive))
		crypto = true;
	showClient(args, crypto, sign, newWindow);
}

uint Application::readTSLVersion(const QString &path)
{
	QFile f(path);
	if(!f.open(QFile::ReadOnly))
		return 0;
	QXmlStreamReader r(&f);
	while(!r.atEnd())
	{
		if(r.readNextStartElement() && r.name() == QLatin1String("TSLSequenceNumber"))
		{
			r.readNext();
			return r.text().toUInt();
		}
	}
	return 0;
}

int Application::run()
{
#ifndef Q_OS_MAC
	if( isRunning() ) return 0;
#endif
	return exec();
}

void Application::showClient(const QStringList &params, bool crypto, bool sign, bool newWindow)
{
	if(sign)
		sign = params.size() != 1 || !CONTAINER_EXT.contains(QFileInfo(params.value(0)).suffix(), Qt::CaseInsensitive);
	QWidget *w = nullptr;
	if(!newWindow && params.isEmpty())
	{
		// If no files selected (e.g. restoring minimized window), select first
		w = qobject_cast<MainWindow*>(mainWindow());
	}
	else if(!newWindow)
	{
		// else select first window with no open files
		for(auto *widget : topLevelWidgets())
		{
			if(auto *main = qobject_cast<MainWindow*>(widget);
				main && main->windowFilePath().isEmpty())
			{
				w = main;
				break;
			}
		}
	}
	if( !w )
	{
		w = new MainWindow();
		QWidget *prev = [w]() -> QWidget* {
			for(QWidget *top: topLevelWidgets())
			{
				QWidget *prev = qobject_cast<MainWindow*>(top);
				if(!prev)
					prev = qobject_cast<FirstRun*>(top);
				if(prev && prev != w && prev->isVisible())
					return prev;
			}
			return {};
		}();
		if(prev)
			w->move(prev->geometry().topLeft() + QPoint(20, 20));
#ifdef Q_OS_LINUX
		else
		{
			if(QScreen *screen = screenAt(w->pos()))
				w->move(screen->availableGeometry().center() - w->frameGeometry().adjusted(0, 0, 10, 40).center());
		}
#endif
	}
#ifdef Q_OS_MAC
	// Required for restoring minimized window on macOS
	w->setWindowState(Qt::WindowActive);
#endif
	w->show();
	w->activateWindow();
	w->raise();
	if( !params.isEmpty() )
		QMetaObject::invokeMethod(w, "open", Q_ARG(QStringList,params), Q_ARG(bool,crypto), Q_ARG(bool,sign));
}

void Application::showWarning( const QString &msg, const digidoc::Exception &e )
{
	digidoc::Exception::ExceptionCode code = digidoc::Exception::General;
	QStringList causes = DigiDoc::parseException(e, code);
	WarningDialog::show(msg, causes.join('\n'));
}

QSigner* Application::signer() const { return d->signer; }

void Application::updateTSLCache(const QDateTime &tslTime)
{
	QString cache = confValue(Application::TSLCache).toString();
	QDir().mkpath(cache);
	const QStringList tsllist = QDir(QStringLiteral(":/TSL/")).entryList();
	for(const QString &file: tsllist)
	{
		if(QFile tl(cache + "/" + file);
			readTSLVersion(":/TSL/" + file) > readTSLVersion(tl.fileName()))
		{
			const QStringList cleanup = QDir(cache, file + QStringLiteral("*")).entryList();
			for(const QString &rm: cleanup)
				QFile::remove(cache + '/' + rm);
			QFile::copy(":/TSL/" + file, tl.fileName());
			tl.setPermissions(QFile::Permissions(0x6444));
			if(tslTime.isValid() && tl.open(QFile::Append))
				tl.setFileTime(tslTime, QFileDevice::FileModificationTime);
		}
	}
}

void Application::waitForTSL( const QString &file )
{
	if(!CONTAINER_EXT.contains(QFileInfo(file).suffix(), Qt::CaseInsensitive))
		return;

	if( d->ready )
		return;

	WaitDialogHider hider;
	QProgressDialog p(tr("Loading TSL lists"), QString(), 0, 0, mainWindow());
	p.setWindowFlags( (Qt::Dialog | Qt::CustomizeWindowHint | Qt::MSWindowsFixedSizeDialogHint ) & ~Qt::WindowTitleHint );
	p.setWindowModality(Qt::WindowModal);
	p.setFixedSize( p.size() );
	if(auto *bar = p.findChild<QProgressBar*>())
		bar->setTextVisible( false );
	p.setMinimumWidth( 300 );
	p.setRange( 0, 100 );
	p.open();
	QTimer t;
	connect(&t, &QTimer::timeout, &p, [&]{
		if(p.value() + 1 == p.maximum())
			p.setValue(0);
		p.setValue( p.value() + 1 );
		t.start(100ms);
	});
	t.start(100ms);
	QEventLoop e;
	connect(this, &Application::TSLLoadingFinished, &e, &QEventLoop::quit);
	if( !d->ready )
		e.exec();
	t.stop();
}
