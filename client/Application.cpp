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

#include "MainWindow.h"
#include "QSigner.h"
#include "QSmartCard.h"
#include "DigiDoc.h"
#include "Styles.h"
#ifdef Q_OS_MAC
#include "MacMenuBar.h"
#else
class MacMenuBar {};
#endif
#include "TokenData.h"
#include "dialogs/FirstRun.h"
#include "dialogs/SettingsDialog.h"
#include "dialogs/WaitDialog.h"
#include "dialogs/WarningDialog.h"

#include <common/Configuration.h>

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
#include <QtCore/QSettings>
#include <QtCore/QStandardPaths>
#include <QtCore/QTimer>
#include <QtCore/QTranslator>
#include <QtCore/QUrl>
#include <QtCore/QUrlQuery>
#include <QtCore/QXmlStreamReader>
#include <QtGui/QDesktopServices>
#include <QtGui/QFileOpenEvent>
#include <QtNetwork/QNetworkProxy>
#include <QtNetwork/QSslCertificate>
#include <QtNetwork/QSslConfiguration>
#include <QtWidgets/QMessageBox>
#include <QtWidgets/QProgressBar>
#include <QtWidgets/QProgressDialog>
#include <QtWidgets/QToolTip>

#ifdef Q_OS_WIN32
#include <QtCore/QLibrary>
#include <qt_windows.h>
#include <MAPI.h>
#endif

const QStringList Application::CONTAINER_EXT {
	QStringLiteral("asice"), QStringLiteral("sce"),
	QStringLiteral("asics"), QStringLiteral("scs"),
	QStringLiteral("bdoc"), QStringLiteral("edoc"),
};

class DigidocConf: public digidoc::XmlConfCurrent
{
public:
	DigidocConf()
	{
		debug = s.value(QStringLiteral("LibdigidocppDebug"), false).toBool();
		s.remove(QStringLiteral("LibdigidocppDebug"));

#ifdef CONFIG_URL
		Configuration::connect(&Configuration::instance(), &Configuration::updateReminder,
				[&](bool /* expired */, const QString & /* title */, const QString &message){
			WarningDialog(message, qApp->activeWindow()).exec();
		});

		reload();
#ifdef Q_OS_MAC
		QTimer::singleShot(0, [] { Configuration::instance().checkVersion(QStringLiteral("QDIGIDOC4")); });
#else
		Configuration::instance().checkVersion(QStringLiteral("QDIGIDOC4"));
#endif
		Configuration::connect(&Configuration::instance(), &Configuration::finished, [&](bool changed, const QString & /*error*/){
			if(changed)
				reload();
		});
#endif
		SettingsDialog::loadProxy(this);
	}

	int logLevel() const override
	{
		return debug ? 4 : digidoc::XmlConfCurrent::logLevel();
	}

	std::string logFile() const override
	{
		return debug ? QStringLiteral("%1/libdigidocpp.log").arg(QDir::tempPath()).toStdString() : digidoc::XmlConfCurrent::logFile();
	}

	std::string proxyHost() const override
	{
		switch(s.value(QStringLiteral("ProxyConfig")).toUInt())
		{
		case 0: return {};
		case 1: return systemProxy().hostName().toStdString();
		default: return s.value(QStringLiteral("ProxyHost"), QString::fromStdString(digidoc::XmlConfCurrent::proxyHost()) ).toString().toStdString();
		}
	}

	std::string proxyPort() const override
	{
		switch(s.value(QStringLiteral("ProxyConfig")).toUInt())
		{
		case 0: return {};
		case 1: return QString::number(systemProxy().port()).toStdString();
		default: return s.value(QStringLiteral("ProxyPort"), QString::fromStdString(digidoc::XmlConfCurrent::proxyPort()) ).toString().toStdString();
		}
	}

	std::string proxyUser() const override
	{
		switch(s.value(QStringLiteral("ProxyConfig")).toUInt())
		{
		case 0: return {};
		case 1: return systemProxy().user().toStdString();
		default: return s.value(QStringLiteral("ProxyUser"), QString::fromStdString(digidoc::XmlConfCurrent::proxyUser()) ).toString().toStdString();
		}
	}

	std::string proxyPass() const override
	{
		switch(s.value(QStringLiteral("ProxyConfig")).toUInt())
		{
		case 0: return {};
		case 1: return systemProxy().password().toStdString();
		default: return s.value(QStringLiteral("ProxyPass"), QString::fromStdString(digidoc::XmlConfCurrent::proxyPass())).toString().toStdString();
		}
	}

#ifdef Q_OS_MAC
	bool proxyTunnelSSL() const override
	{ return s.value(QStringLiteral("ProxyTunnelSSL"), digidoc::XmlConfCurrent::proxyTunnelSSL()).toBool(); }
	bool PKCS12Disable() const override
	{ return s.value(QStringLiteral("PKCS12Disable"), digidoc::XmlConfCurrent::PKCS12Disable()).toBool(); }
	std::string TSLCache() const override
	{ return Application::groupContainer().toStdString(); }
	bool TSLOnlineDigest() const override
	{ return s.value(QStringLiteral("TSLOnlineDigest"), digidoc::XmlConfCurrent::TSLOnlineDigest()).toBool(); }

	void setProxyHost( const std::string &host ) override
	{ SettingsDialog::setValueEx(QStringLiteral("ProxyHost"), QString::fromStdString( host )); }
	void setProxyPort( const std::string &port ) override
	{ SettingsDialog::setValueEx(QStringLiteral("ProxyPort"), QString::fromStdString( port )); }
	void setProxyUser( const std::string &user ) override
	{ SettingsDialog::setValueEx(QStringLiteral("ProxyUser"), QString::fromStdString( user )); }
	void setProxyPass( const std::string &pass ) override
	{ SettingsDialog::setValueEx(QStringLiteral("ProxyPass"), QString::fromStdString( pass )); }
	void setProxyTunnelSSL( bool enable ) override
	{ SettingsDialog::setValueEx(QStringLiteral("ProxyTunnelSSL"), enable, digidoc::XmlConfCurrent::proxyTunnelSSL()); }
	void setPKCS12Cert( const std::string & /*cert*/) override {}
	void setPKCS12Pass( const std::string & /*pass*/) override {}
	void setPKCS12Disable( bool disable ) override
	{ SettingsDialog::setValueEx(QStringLiteral("PKCS12Disable"), disable, digidoc::XmlConfCurrent::PKCS12Disable()); }
	void setTSLOnlineDigest( bool enable ) override
	{ SettingsDialog::setValueEx(QStringLiteral("TSLOnlineDigest"), enable, digidoc::XmlConfCurrent::TSLOnlineDigest()); }
#endif

	std::string TSUrl() const override
	{
		if(s.value(QStringLiteral("TSA-URL-CUSTOM"), s.contains(QStringLiteral("TSA-URL"))).toBool())
			return valueUserScope(QStringLiteral("TSA-URL"), digidoc::XmlConfCurrent::TSUrl());
		return valueSystemScope(QStringLiteral("TSA-URL"), digidoc::XmlConfCurrent::TSUrl());
	}
	void setTSUrl(const std::string &url) override
	{ SettingsDialog::setValueEx(QStringLiteral("TSA-URL"), QString::fromStdString(url)); }

	std::string TSLUrl() const override { return valueSystemScope(QStringLiteral("TSL-URL"), digidoc::XmlConfCurrent::TSLUrl()); }
	digidoc::X509Cert verifyServiceCert() const override
	{
		QByteArray cert = QByteArray::fromBase64(obj.value(QStringLiteral("SIVA-CERT")).toString().toLatin1());
		if(cert.isEmpty())
			return digidoc::XmlConfCurrent::verifyServiceCert();
		return digidoc::X509Cert((const unsigned char*)cert.constData(), size_t(cert.size()));
	}
	std::vector<digidoc::X509Cert> verifyServiceCerts() const override
	{
		std::vector<digidoc::X509Cert> list;
		list.push_back(verifyServiceCert());
		for(const QJsonValue &cert: obj.value(QStringLiteral("CERT-BUNDLE")).toArray())
		{
			QByteArray der = QByteArray::fromBase64(cert.toString().toLatin1());
			list.emplace_back((const unsigned char*)der.constData(), size_t(der.size()));
		}
		return list;
	}
	std::string verifyServiceUri() const override { return valueSystemScope(QStringLiteral("SIVA-URL"), digidoc::XmlConfCurrent::verifyServiceUri()); }
	std::vector<digidoc::X509Cert> TSLCerts() const override
	{
		std::vector<digidoc::X509Cert> tslcerts;
		for(const QJsonValue &val: obj.value(QStringLiteral("TSL-CERTS")).toArray())
		{
			QByteArray cert = QByteArray::fromBase64(val.toString().toLatin1());
			tslcerts.emplace_back((const unsigned char*)cert.constData(), size_t(cert.size()));
		}
		return tslcerts.empty() ? digidoc::XmlConfCurrent::TSLCerts() : tslcerts;
	}
	std::string ocsp(const std::string &issuer) const override
	{
		QJsonObject ocspissuer = obj.value(QStringLiteral("OCSP-URL-ISSUER")).toObject();
		for(QJsonObject::const_iterator i = ocspissuer.constBegin(); i != ocspissuer.constEnd(); ++i)
		{
			if(issuer == i.key().toStdString())
				return i.value().toString().toStdString();
		}
		return obj.value(QStringLiteral("OCSP-URL")).toString(QString::fromStdString(digidoc::XmlConfCurrent::ocsp(issuer))).toStdString();
	}

	bool TSLAllowExpired() const override
	{
		static enum {
			Undefined,
			Approved,
			Rejected
		} status = Undefined;
		if(status == Undefined)
		{
			QEventLoop e;
			QMetaObject::invokeMethod( qApp, "showTSLWarning", Q_ARG(QEventLoop*,&e) );
			e.exec();
			status = Approved;
		}
		return status == Approved;
	}

private:
#ifdef CONFIG_URL
	void reload()
	{
		obj = Configuration::instance().object();
		if(s.value(QStringLiteral("TSA-URL")) == obj.value(QStringLiteral("TSA-URL")))
			s.remove(QStringLiteral("TSA-URL")); // Cleanup user conf if it is default url
		QList<QSslCertificate> list;
		for(const QJsonValue &cert: obj.value(QStringLiteral("CERT-BUNDLE")).toArray())
			list << QSslCertificate(QByteArray::fromBase64(cert.toString().toLatin1()), QSsl::Der);
		QSslConfiguration ssl = QSslConfiguration::defaultConfiguration();
		ssl.setCaCertificates(list);
		QSslConfiguration::setDefaultConfiguration(ssl);
	}
#endif

	QNetworkProxy systemProxy() const
	{
		for(const QNetworkProxy &proxy: QNetworkProxyFactory::systemProxyForQuery())
		{
			if(proxy.type() == QNetworkProxy::HttpProxy)
				return proxy;
		}
		return {};
	}

	std::string valueSystemScope(const QString &key, const std::string &defaultValue) const
	{
		return obj.value(key).toString(QString::fromStdString(defaultValue)).toStdString();
	}

	std::string valueUserScope(const QString &key, const std::string &defaultValue) const
	{
		return s.value(key, obj.value(key).toString(QString::fromStdString(defaultValue))).toString().toStdString();
	}

	QSettings s;
	bool	debug = false;
public:
	QJsonObject obj;
};

class Application::Private
{
public:
	QAction		*closeAction = nullptr, *newClientAction = nullptr, *newCryptoAction = nullptr, *helpAction = nullptr;
	MacMenuBar	*bar = nullptr;
	QSigner		*signer = nullptr;

	QTranslator	appTranslator, commonTranslator, qtTranslator;
	QString		lang;
	QTimer		lastWindowTimer;
	volatile bool ready = false;
#ifdef Q_OS_WIN
	QStringList	tempFiles;
#endif // Q_OS_WIN
};

Application::Application( int &argc, char **argv )
#ifdef Q_OS_MAC
	: Common(argc, argv, QStringLiteral("qdigidoc4"), QStringLiteral(":/images/Icon.svg"))
#else
	: Common(argc, argv, QStringLiteral("qdigidoc4"), QStringLiteral(":/images/digidoc_128.png"))
#endif
	, d(new Private)
{
	qRegisterMetaType<TokenData>("TokenData");
	qRegisterMetaType<QSmartCardData>("QSmartCardData");
	QToolTip::setFont(Styles::font(Styles::Regular, 14));

	QStringList args = arguments();
	args.removeFirst();
#ifndef Q_OS_MAC
	if( isRunning() )
	{
		sendMessage(args.join(QStringLiteral("\", \"")));
		return;
	}
	connect( this, SIGNAL(messageReceived(QString)), SLOT(parseArgs(QString)) );
#endif

	QDesktopServices::setUrlHandler(QStringLiteral("browse"), this, "browse");
	QDesktopServices::setUrlHandler(QStringLiteral("mailto"), this, "mailTo");

	installTranslator( &d->appTranslator );
	installTranslator( &d->commonTranslator );
	installTranslator( &d->qtTranslator );
	loadTranslation( Common::language() );

	// Actions
	d->closeAction = new QAction( tr("Close Window"), this );
	d->closeAction->setShortcut( Qt::CTRL + Qt::Key_W );
	connect(d->closeAction, &QAction::triggered, this, &Application::closeWindow);
	d->newClientAction = new QAction( tr("New Window"), this );
	d->newClientAction->setShortcut( Qt::CTRL + Qt::Key_N );
	connect(d->newClientAction, &QAction::triggered, this, [&]{ showClient({}, false, false, true); });

	// This is needed to release application from memory (Windows)
	setQuitOnLastWindowClosed( true ); 
	d->lastWindowTimer.setSingleShot(true);
	connect(&d->lastWindowTimer, &QTimer::timeout, []{ if(topLevelWindows().isEmpty()) quit(); });
	connect(this, &Application::lastWindowClosed, [&]{ d->lastWindowTimer.start(10*1000); });

#ifdef Q_OS_MAC
	d->bar = new MacMenuBar;
	d->bar->addAction( MacMenuBar::AboutAction, this, SLOT(showAbout()) );
	d->bar->addAction( MacMenuBar::PreferencesAction, this, SLOT(showSettings()) );
	d->bar->fileMenu()->addAction( d->newClientAction );
	d->bar->fileMenu()->addAction( d->newCryptoAction );
	d->bar->fileMenu()->addAction( d->closeAction );
	d->bar->dockMenu()->addAction( d->newClientAction );
	d->bar->dockMenu()->addAction( d->newCryptoAction );
	d->helpAction = d->bar->helpMenu()->addAction(tr("DigiDoc4 Client Help"), this, &Application::openHelp);
#endif

	QSigner::ApiType api = QSigner::PKCS11;
#ifdef Q_OS_WIN
	api = QSigner::ApiType(QSettings().value("tokenBackend", QSigner::CNG).toUInt());
	if( args.contains("-cng") )
		api = QSigner::CNG;
	if( args.contains("-capi") )
		api = QSigner::CAPI;
	if( args.contains("-pkcs11") )
		api = QSigner::PKCS11;
#endif

	try
	{
		digidoc::Conf::init( new DigidocConf );
		d->signer = new QSigner(api, this);
		QString cache = confValue(TSLCache).toString();
		QDir().mkpath( cache );
		QDateTime tslTime = QDateTime::currentDateTimeUtc().addDays(-7);
		for(const QString &file: QDir(QStringLiteral(":/TSL/")).entryList())
		{
			QFile tl(cache + "/" + file);
			if(!tl.exists() ||
				readTSLVersion(":/TSL/" + file) > readTSLVersion(tl.fileName()))
			{
				tl.remove();
				QFile::copy(":/TSL/" + file, tl.fileName());
				tl.setPermissions(QFile::Permissions(0x6444));
#if QT_VERSION >= QT_VERSION_CHECK(5, 10, 0)
				if(tl.open(QFile::Append))
					tl.setFileTime(tslTime, QFileDevice::FileModificationTime);
#endif
			}
		}

		qRegisterMetaType<QEventLoop*>("QEventLoop*");
		digidoc::initialize(applicationName().toUtf8().constData(), QStringLiteral("%1/%2 (%3)")
			.arg(applicationName(), applicationVersion(), applicationOs()).toUtf8().constData(),
			[](const digidoc::Exception *ex) {
				qDebug() << "TSL loading finished";
				Q_EMIT qApp->TSLLoadingFinished();
				qApp->d->ready = true;
				if(ex) {
					QStringList causes;
					digidoc::Exception::ExceptionCode code = digidoc::Exception::General;
					DigiDoc::parseException(*ex, causes, code);
					QMetaObject::invokeMethod( qApp, "showWarning",
						Q_ARG(QString,tr("Failed to initalize.")), Q_ARG(QString,causes.join('\n')) );
				}
			}
		);
	}
	catch( const digidoc::Exception &e )
	{
		showWarning( tr("Failed to initalize."), e );
		setQuitOnLastWindowClosed( true );
		return;
	}

	QTimer::singleShot(0, [this] {
		if(QSettings().value(QStringLiteral("showIntro"), true).toBool())
		{
			QSettings().setValue(QStringLiteral("showIntro"), false);
			FirstRun dlg;
			connect(&dlg, &FirstRun::langChanged, this, [this](const QString& lang) { loadTranslation( lang ); });
			dlg.exec();
		}
#ifdef Q_OS_MAC
		if(QSettings().value(QStringLiteral("plugins")).isNull())
		{
			QTimer::singleShot(0, [] {
				WarningDialog dlg(tr(
					"In order to authenticate and sign in e-services with an ID-card you need to install the web browser components."));
				dlg.setCancelText(tr("Ignore forever").toUpper());
				dlg.addButton(tr("Remind later").toUpper(), QMessageBox::Ignore);
				dlg.addButton(tr("Install").toUpper(), QMessageBox::Open);
				switch(dlg.exec())
				{
				case QMessageBox::Open: QDesktopServices::openUrl(tr("https://www.id.ee/en/article/install-id-software/")); break;
				case QMessageBox::Ignore: break;
				default: QSettings().setValue(QStringLiteral("plugins"), "ignore");
				}
			});
		}
#endif
	});

	if( !args.isEmpty() || topLevelWindows().isEmpty() )
		parseArgs( args );
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
	if( QtLocalPeer *obj = findChild<QtLocalPeer*>() )
		delete obj;
#else
	deinitMacEvents();
#endif
	delete d->bar;
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

void Application::activate( QWidget *w )
{
#ifdef Q_OS_MAC
	w->installEventFilter( d->bar );
	// Required for restoring minimized window on macOS
	w->setWindowState(Qt::WindowActive);
#endif
	w->addAction( d->closeAction );
	w->activateWindow();
	w->show();
	w->raise();
}

#ifdef Q_OS_WIN
void Application::addTempFile(const QString &file)
{
	d->tempFiles << file;
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

void Application::clearConfValue( ConfParameter parameter )
{
	try
	{
		digidoc::XmlConfCurrent *i = dynamic_cast<digidoc::XmlConfCurrent*>(digidoc::Conf::instance());
		if(!i)
			return;
		switch( parameter )
		{
		case PKCS12Cert: i->setPKCS12Cert( digidoc::Conf().PKCS12Cert() ); break;
		case PKCS12Pass: i->setPKCS12Pass( digidoc::Conf().PKCS12Pass() ); break;
		case ProxyHost:
		case ProxyPort:
		case ProxyUser:
		case ProxyPass:
		case ProxySSL:
		case PKCS12Disable:
		case TSLOnlineDigest:
		case SiVaUrl:
		case TSAUrl:
		case TSLCerts:
		case TSLUrl:
		case TSLCache: break;
		}
	}
	catch( const digidoc::Exception &e )
	{
		showWarning(tr("Caught exception!"), e);
	}
}

void Application::closeWindow()
{
#ifndef Q_OS_MAC
	if( MainWindow *w = qobject_cast<MainWindow*>(activeWindow()) )
		w->close();
	else
#endif
	if( QDialog *d = qobject_cast<QDialog*>(activeWindow()) )
		d->reject();
	else if( QWidget *w = qobject_cast<QWidget*>(activeWindow()) )
		w->close();
}

QVariant Application::confValue( ConfParameter parameter, const QVariant &value )
{
	DigidocConf *i = nullptr;
	try { i = static_cast<DigidocConf*>(digidoc::Conf::instance()); }
	catch( const digidoc::Exception & ) { return value; }

	QByteArray r;
	switch( parameter )
	{
	case SiVaUrl: r = i->verifyServiceUri().c_str(); break;
	case ProxyHost: r = i->proxyHost().c_str(); break;
	case ProxyPort: r = i->proxyPort().c_str(); break;
	case ProxyUser: r = i->proxyUser().c_str(); break;
	case ProxyPass: r = i->proxyPass().c_str(); break;
	case ProxySSL: return i->proxyTunnelSSL();
	case PKCS12Cert: r = i->PKCS12Cert().c_str(); break;
	case PKCS12Pass: r = i->PKCS12Pass().c_str(); break;
	case PKCS12Disable: return i->PKCS12Disable();
	case TSAUrl: r = i->TSUrl().c_str(); break;
	case TSLUrl: r = i->TSLUrl().c_str(); break;
	case TSLCache: r = i->TSLCache().c_str(); break;
	case TSLCerts:
	{
		QList<QSslCertificate> list;
		for(const digidoc::X509Cert &cert: i->TSLCerts())
		{
			std::vector<unsigned char> v = cert;
			if(!v.empty())
				list << QSslCertificate(QByteArray::fromRawData((const char*)v.data(), int(v.size())), QSsl::Der);
		}
		return QVariant::fromValue(list);
	}
	case TSLOnlineDigest: return i->TSLOnlineDigest();
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
		QTimer::singleShot(0, [this, fileName] {
			parseArgs({ fileName });
		});
		return true;
	}
#ifdef Q_OS_MAC
	// Load here because cocoa NSApplication overides events
	case QEvent::ApplicationActivate:
		initMacEvents();
		return Common::event(event);
#endif
	default: return Common::event(event);
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
	QSettings().setValue(QStringLiteral("Language"), d->lang = lang);

	if(lang == QLatin1String("en")) QLocale::setDefault(QLocale(QLocale::English, QLocale::UnitedKingdom));
	else if(lang == QLatin1String("ru")) QLocale::setDefault(QLocale( QLocale::Russian, QLocale::RussianFederation));
	else QLocale::setDefault(QLocale(QLocale::Estonian, QLocale::Estonia));

	void(d->appTranslator.load(QStringLiteral(":/translations/") + lang));
	void(d->commonTranslator.load(QStringLiteral(":/translations/common_") + lang));
	void(d->qtTranslator.load(QStringLiteral(":/translations/qt_") + lang));
	if( d->closeAction ) d->closeAction->setText( tr("Close Window") );
	if( d->newClientAction ) d->newClientAction->setText( tr("New Window") );
	if( d->newCryptoAction ) d->newCryptoAction->setText( tr("New Crypto window") );
	if(d->helpAction) d->helpAction->setText(tr("DigiDoc4 Client Help"));
}

#ifndef Q_OS_MAC
void Application::mailTo( const QUrl &url )
{
	QUrlQuery q(url);
#if defined(Q_OS_WIN)
	QString file = q.queryItemValue( "attachment", QUrl::FullyDecoded );
	QLibrary lib("mapi32");
	if( LPMAPISENDMAILW mapi = LPMAPISENDMAILW(lib.resolve("MAPISendMailW")) )
	{
		QString filePath = QDir::toNativeSeparators( file );
		QString fileName = QFileInfo( file ).fileName();
		QString subject = q.queryItemValue( "subject", QUrl::FullyDecoded );
		MapiFileDescW doc = {};
		doc.nPosition = -1;
		doc.lpszPathName = PWSTR(filePath.utf16());
		doc.lpszFileName = PWSTR(fileName.utf16());
		MapiMessageW message = {};
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
	else if( LPMAPISENDMAIL mapi = LPMAPISENDMAIL(lib.resolve("MAPISendMail")) )
	{
		QByteArray filePath = QDir::toNativeSeparators( file ).toLocal8Bit();
		QByteArray fileName = QFileInfo( file ).fileName().toLocal8Bit();
		QByteArray subject = q.queryItemValue( "subject", QUrl::FullyDecoded ).toLocal8Bit();
		MapiFileDesc doc = {};
		doc.nPosition = -1;
		doc.lpszPathName = LPSTR(filePath.constData());
		doc.lpszFileName = LPSTR(fileName.constData());
		MapiMessage message = {};
		message.lpszSubject = LPSTR(subject.constData());
		message.lpszNoteText = LPSTR("");
		message.nFileCount = 1;
		message.lpFiles = lpMapiFileDesc(&doc);
		switch( mapi( NULL, 0, &message, MAPI_LOGON_UI|MAPI_DIALOG, 0 ) )
		{
		case SUCCESS_SUCCESS:
		case MAPI_E_USER_ABORT:
		case MAPI_E_LOGIN_FAILURE:
			return;
		default: break;
		}
	}
#elif defined(Q_OS_LINUX)
	QByteArray thunderbird;
	QProcess p;
	QStringList env = QProcess::systemEnvironment();
	if( env.indexOf( QRegExp("KDE_FULL_SESSION.*") ) != -1 )
	{
		p.start("kreadconfig", {
			"--file", "emaildefaults",
			"--group", "PROFILE_Default",
			"--key", "EmailClient"});
		p.waitForFinished();
		QByteArray data = p.readAllStandardOutput().trimmed();
		if( data.contains("thunderbird") )
			thunderbird = data;
	}
	else if( env.indexOf( QRegExp("GNOME_DESKTOP_SESSION_ID.*") ) != -1 )
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
	QWidget* win = activeWindow();
	QWidget* first = nullptr;
	QWidget* root = nullptr;

	if (!win)
	{	
		// Prefer main window; on Mac also the menu is top level window
		for (QWidget *widget: qApp->topLevelWidgets())
		{
			if (widget->isWindow())
			{
				if (!first)
					first = widget;

				if(qobject_cast<MainWindow*>(widget))
				{
					win = widget;
					break;
				}
			}
		}
	}

	if(!win)
		win = first;

	while(win)
	{
		root = win;
		win = win->parentWidget();
	}

	return root;
}

void Application::migrateSettings()
{
#ifdef Q_OS_MAC
	QSettings oldOrgSettings;
	QSettings oldAppSettings;
	QSettings newSettings;
#else
	QSettings dd3Settings(QStringLiteral("Estonian ID Card"), QStringLiteral("qdigidocclient"));
	QSettings oldOrgSettings(QStringLiteral("Estonian ID Card"), {});
	QSettings oldAppSettings(QStringLiteral("Estonian ID Card"), QStringLiteral("qdigidoc4"));
	QSettings newSettings;
#endif
	newSettings.remove(QStringLiteral("Client/Type"));

	if(newSettings.value(QStringLiteral("SettingsMigrated"), false).toBool())
		return;

#ifdef Q_OS_WIN
	QSettings reg(QStringLiteral("HKEY_CURRENT_USER\\Software"), QSettings::NativeFormat);
	if(!reg.childGroups().contains(QStringLiteral("Estonian ID Card")))
	{
		newSettings.setValue(QStringLiteral("SettingsMigrated"), true);
		return;
	}

	if(!reg.childGroups().contains(QStringLiteral("RIA/Digidoc4 Client")))
	{
		if(reg.contains(QStringLiteral("RIA/DigiDoc4 Client/DesktopShortcut4")))
			newSettings.setValue(QStringLiteral("DesktopShortcut4"), reg.value(QStringLiteral("RIA/DigiDoc4 Client/DesktopShortcut4")));

		if(reg.contains(QStringLiteral("RIA/DigiDoc4 Client/ProgramMenuDir4")))
			newSettings.setValue(QStringLiteral("ProgramMenuDir4"), reg.value(QStringLiteral("RIA/DigiDoc4 Client/ProgramMenuDir4")));

		reg.remove(QStringLiteral("RIA/DigiDoc4 Client"));
	}
#endif

	static const QVector<QPair<QString,QString>> orgOldNewKeys {
		{"showIntro", "showIntro"},
		{"PKCS12Disable","PKCS12Disable"},
		{"Client/MobileCode","MobileCode"},
		{"Client/MobileNumber","MobileNumber"},
		{"ProxyHost","ProxyHost"},
		{"ProxyPass","ProxyPass"},
		{"ProxyPort","ProxyPort"},
		{"ProxyUser","ProxyUser"},
		{"Client/City","City"},
		{"Client/Country","Country"},
		{"Client/Zip","Zip"},
		{"Client/Role","Role"},
		{"Client/State","State"},
		{"Client/Resolution","Resolution"},
		{"Main/language","Language"},
		{"Client/DefaultDir","DefaultDir"},
		{"ProxyTunnelSSL","ProxyTunnelSSL"}
	};

	static const QVector<QPair<QString,QString>> appOldNewKeys {
		{"TSLOnlineDigest", "TSLOnlineDigest"},
		{"Client/proxyConfig", "ProxyConfig"},
		{"lastPath", "lastPath"},
		{"LastCheck", "LastCheck"},
		{"Client/RoleAddressInfo", "RoleAddressInfo"},
		{"Client/ShowPrintSummary", "ShowPrintSummary"},
		{"TSA-URL", "TSA-URL"},
		{"MobileSettings", "MobileSettings"},
		{"tokenBackend", "tokenBackend"},
		{"cdocwithddoc", "cdocwithddoc"}
	};

	for(const QPair<QString,QString> &keypairs: orgOldNewKeys) {
		const QString &oldKey = keypairs.first;
		const QString &newKey = keypairs.second;

		if(oldOrgSettings.contains(oldKey)){
			newSettings.setValue(newKey, oldOrgSettings.value(oldKey));
			
#ifdef Q_OS_MAC
			if(oldKey != newKey)
				oldAppSettings.remove(oldKey);
#endif
		}
	}

	for(const QPair<QString,QString> &keypairs: appOldNewKeys) {
		const QString &oldKey = keypairs.first;
		const QString &newKey = keypairs.second;

		if(oldAppSettings.contains(oldKey)){
			newSettings.setValue(newKey, oldAppSettings.value(oldKey));

#ifdef Q_OS_MAC
			if(oldKey != newKey)
				oldAppSettings.remove(oldKey);
#endif
		}
	}

	for(const QString &key: oldAppSettings.allKeys()){
		if(key.startsWith(QStringLiteral("AccessCertUsage")))
			newSettings.setValue(key, oldAppSettings.value(key));
	}

	newSettings.setValue(QStringLiteral("SettingsMigrated"), true);

#ifndef Q_OS_MAC
	QString key = "proxyConfig";
	if(!newSettings.contains("showIntro") && !newSettings.contains("ProxyConfig") && dd3Settings.contains(key))
	{
		newSettings.setValue("ProxyConfig", dd3Settings.value(key));
		SettingsDialog::loadProxy(digidoc::Conf::instance());
	}

	oldOrgSettings.clear();
	oldAppSettings.clear();
	dd3Settings.clear();

#ifdef Q_OS_WIN
	reg.remove(QStringLiteral("Estonian ID Card"));
#endif
#endif

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
		showWarning(tr("Added file(s) exceeds the maximum size limit of the container(120MB)."), QString::fromLocal8Bit(e.what()));
	}
	catch(...)
	{
		showWarning( tr("Caught exception!") );
	}

	return false;
}

void Application::openHelp()
{
	QString lang = language();
	QUrl u(QStringLiteral("https://www.id.ee/id-abikeskus/"));
	if(lang == QLatin1String("en")) u = QStringLiteral("https://www.id.ee/en/id-help/");
	if(lang == QLatin1String("ru")) u = QStringLiteral("https://www.id.ee/ru/id-pomoshh/");
	QDesktopServices::openUrl(u);
}

void Application::parseArgs( const QString &msg )
{
	QStringList params;


#if QT_VERSION > QT_VERSION_CHECK(5, 14, 0)
	for(const QString &param: msg.split(QStringLiteral("\", \""), Qt::SkipEmptyParts))
#else
	for(const QString &param: msg.split(QStringLiteral("\", \""), QString::SkipEmptyParts))
#endif
	{
		QUrl url( param, QUrl::StrictMode );
		params << (param != QLatin1String("-crypto") && !url.toLocalFile().isEmpty() ? url.toLocalFile() : param);
	}
	parseArgs( params );
}

void Application::parseArgs( const QStringList &args )
{
	bool crypto = args.contains(QLatin1String("-crypto"));
	bool sign = args.contains(QLatin1String("-sign"));
	bool newWindow = args.contains(QLatin1String("-newWindow"));
	QStringList params = args;
	params.removeAll(QStringLiteral("-sign"));
	params.removeAll(QStringLiteral("-crypto"));
	params.removeAll(QStringLiteral("-newWindow"));
	params.removeAll(QStringLiteral("-capi"));
	params.removeAll(QStringLiteral("-cng"));
	params.removeAll(QStringLiteral("-pkcs11"));

	QString suffix = params.size() == 1 ? QFileInfo(params.value(0)).suffix() : QString();
	showClient(params, crypto || (suffix.compare(QLatin1String("cdoc"), Qt::CaseInsensitive) == 0), sign, newWindow);
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
};

int Application::run()
{
#ifndef Q_OS_MAC
	if( isRunning() ) return 0;
#endif
	return exec();
}

void Application::setConfValue( ConfParameter parameter, const QVariant &value )
{
	try
	{
		digidoc::XmlConfCurrent *i = dynamic_cast<digidoc::XmlConfCurrent*>(digidoc::Conf::instance());
		if(!i)
			return;
		QByteArray v = value.toString().toUtf8();
		switch( parameter )
		{
		case ProxyHost: i->setProxyHost( v.isEmpty()? std::string() :  v.constData() ); break;
		case ProxyPort: i->setProxyPort( v.isEmpty()? std::string() : v.constData() ); break;
		case ProxyUser: i->setProxyUser( v.isEmpty()? std::string() : v.constData() ); break;
		case ProxyPass: i->setProxyPass( v.isEmpty()? std::string() : v.constData() ); break;
		case ProxySSL: i->setProxyTunnelSSL( value.toBool() ); break;
		case PKCS12Cert: i->setPKCS12Cert( v.isEmpty()? std::string() : v.constData() ); break;
		case PKCS12Pass: i->setPKCS12Pass( v.isEmpty()? std::string() : v.constData() ); break;
		case PKCS12Disable: i->setPKCS12Disable( value.toBool() ); break;
		case TSLOnlineDigest: i->setTSLOnlineDigest( value.toBool() ); break;
		case TSAUrl: i->setTSUrl(v.isEmpty()? std::string() : v.constData()); break;
		case SiVaUrl:
		case TSLCerts:
		case TSLUrl:
		case TSLCache: break;
		}
	}
	catch( const digidoc::Exception &e )
	{
		showWarning(tr("Caught exception!"), e);
	}
}

void Application::showAbout()
{
	if(MainWindow *w = qobject_cast<MainWindow*>(qApp->mainWindow()))
		w->showSettings(SettingsDialog::LicenseSettings);
}

void Application::showSettings()
{
	if(MainWindow *w = qobject_cast<MainWindow*>(qApp->mainWindow()))
		w->showSettings(SettingsDialog::GeneralSettings);
}

void Application::showClient(const QStringList &params, bool crypto, bool sign, bool newWindow)
{
	if(sign)
		sign = !(params.size() == 1 && CONTAINER_EXT.contains(QFileInfo(params.value(0)).suffix(), Qt::CaseInsensitive));
	QWidget *w = nullptr;
	if(!newWindow && params.isEmpty())
	{
		// If no files selected (e.g. restoring minimized window), select first
		w = qobject_cast<MainWindow*>(qApp->mainWindow());
	}
	else if(!newWindow)
	{
		// else select first window with no open files
		MainWindow *main = qobject_cast<MainWindow*>(qApp->uniqueRoot());
		if(main && main->windowFilePath().isEmpty())
			w = main;
	}
	if( !w )
	{
		QSettings settings;
		migrateSettings();

		w = new MainWindow();
		QWidgetList list = topLevelWidgets();
		for(int i = list.size() - 1; i >= 0; --i)
		{
			MainWindow *prev = qobject_cast<MainWindow*>(list[i]);
			if(prev && prev != w && prev->isVisible())
				w->move(prev->geometry().topLeft() + QPoint(20, 20));
		}
	}
	if( !params.isEmpty() )
		QMetaObject::invokeMethod(w, "open", Q_ARG(QStringList,params), Q_ARG(bool,crypto), Q_ARG(bool,sign));
	activate( w );
}

void Application::showTSLWarning(QEventLoop *e)
{
	showWarning( tr(
		"The renewal of Trust Service status List, used for digital signature validation, has failed. "
		"Please check your internet connection and make sure you have the latest ID-software version "
		"installed. An expired Trust Service List (TSL) will be used for signature validation. "
		"<a href=\"https://www.id.ee/en/article/digidoc4-message-updating-the-list-of-trusted-certificates-was-unsuccessful/\">Additional information</a>") );
	e->exit();
}

void Application::showWarning( const QString &msg, const digidoc::Exception &e )
{
	QStringList causes;
	digidoc::Exception::ExceptionCode code = digidoc::Exception::General;
	DigiDoc::parseException(e, causes, code);
	WarningDialog(msg, causes.join('\n'), qApp->mainWindow()).exec();
}

void Application::showWarning( const QString &msg, const QString &details )
{
	WarningDialog(msg, details, qApp->mainWindow()).exec();
}

QSigner* Application::signer() const { return d->signer; }

QWidget* Application::uniqueRoot()
{
	MainWindow* root = nullptr;

	// Return main window if only one main window is opened
	for(auto w : topLevelWidgets())
	{
		if(MainWindow *r = qobject_cast<MainWindow*>(w))
		{
			if(root)
				return nullptr;
			root = r;
		}
	}

	return root;
}

void Application::waitForTSL( const QString &file )
{
	if(!CONTAINER_EXT.contains(QFileInfo(file).suffix(), Qt::CaseInsensitive))
		return;

	if( d->ready )
		return;

	WaitDialogHider hider;
	QProgressDialog p( tr("Loading TSL lists"), QString(), 0, 0, qApp->mainWindow() );
	p.setWindowFlags( (Qt::Dialog | Qt::CustomizeWindowHint | Qt::MSWindowsFixedSizeDialogHint ) & ~Qt::WindowTitleHint );
	p.setWindowModality(Qt::WindowModal);
	p.setFixedSize( p.size() );
	if( QProgressBar *bar = p.findChild<QProgressBar*>() )
		bar->setTextVisible( false );
	p.setMinimumWidth( 300 );
	p.setRange( 0, 100 );
	p.open();
	QTimer t;
	connect(&t, &QTimer::timeout, &p, [&]{
		if(p.value() + 1 == p.maximum())
			p.setValue(0);
		p.setValue( p.value() + 1 );
		t.start( 100 );
	});
	t.start( 100 );
	QEventLoop e;
	connect(this, &Application::TSLLoadingFinished, &e, &QEventLoop::quit);
	if( !d->ready )
		e.exec();
	t.stop();
}
