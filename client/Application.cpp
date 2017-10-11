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
#include "DigiDoc.h"
#include "dialogs/FirstRun.h"

#include <common/AboutDialog.h>
#include <common/Configuration.h>
#include <common/Settings.h>
#include <common/SslCertificate.h>

#include <digidocpp/Container.h>
#include <digidocpp/XmlConf.h>
#include <digidocpp/crypto/X509Cert.h>

#include "qtsingleapplication/src/qtlocalpeer.h"

#include <QtCore/QFileInfo>
#include <QtCore/QProcess>
#include <QtCore/QJsonArray>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <QtCore/QSysInfo>
#include <QtCore/QTimer>
#include <QtCore/QTranslator>
#include <QtCore/QUrl>
#include <QtCore/QUrlQuery>
#include <QtCore/QXmlStreamReader>
#include <QtGui/QDesktopServices>
#include <QtGui/QFileOpenEvent>
#include <QtNetwork/QNetworkProxy>
#include <QtNetwork/QSslConfiguration>
#include <QtWidgets/QMessageBox>
#include <QtWidgets/QProgressBar>
#include <QtWidgets/QProgressDialog>
#include <QtWidgets/QAction>

#if defined(Q_OS_MAC)
#include <common/MacMenuBar.h>
#else
class MacMenuBar;
#endif

#ifdef Q_OS_WIN32
#include <QtCore/QLibrary>
#include <qt_windows.h>
#include <mapi.h>
#endif

class DigidocConf: public digidoc::XmlConfCurrent
{
public:
	DigidocConf()
		: digidoc::XmlConfCurrent()
		, s2(QCoreApplication::instance()->applicationName())
	{
		reload();
#ifdef Q_OS_MAC
		QTimer *t = new QTimer();
		t->setSingleShot(true);
		QTimer::connect(t, &QTimer::timeout, [=] {
			t->deleteLater();
			Configuration::instance().checkVersion("QDIGIDOC4");
		});
		t->start(0);
#else
		Configuration::instance().checkVersion("QDIGIDOC4");
#endif
		Configuration::connect(&Configuration::instance(), &Configuration::finished, [&](bool changed, const QString &){
			if(changed)
				reload();
		});
		s.beginGroup( "Client" );
		// TODO PROXY
		// SettingsDialog::loadProxy(this);
	}

	std::string proxyHost() const override
	{
		switch(s2.value("proxyConfig").toUInt())
		{
		case 0: return std::string();
		case 1: return systemProxy().hostName().toStdString();
		default: return s.value( "ProxyHost", QString::fromStdString(digidoc::XmlConfCurrent::proxyHost()) ).toString().toStdString();
		}
	}

	std::string proxyPort() const override
	{
		switch(s2.value("proxyConfig").toUInt())
		{
		case 0: return std::string();
		case 1: return QString::number(systemProxy().port()).toStdString();
		default: return s.value( "ProxyPort", QString::fromStdString(digidoc::XmlConfCurrent::proxyPort()) ).toString().toStdString();
		}
	}

	std::string proxyUser() const override
	{
		switch(s2.value("proxyConfig").toUInt())
		{
		case 0: return std::string();
		case 1: return systemProxy().user().toStdString();
		default: return s.value( "ProxyUser", QString::fromStdString(digidoc::XmlConfCurrent::proxyUser()) ).toString().toStdString();
		}
	}

	std::string proxyPass() const override
	{
		switch(s2.value("proxyConfig").toUInt())
		{
		case 0: return std::string();
		case 1: return systemProxy().password().toStdString();
		default: return s.value( "ProxyPass", QString::fromStdString(digidoc::XmlConfCurrent::proxyPass()) ).toString().toStdString();
		}
	}

#ifdef Q_OS_MAC
	bool proxyTunnelSSL() const override
	{ return s.value( "ProxyTunnelSSL", digidoc::XmlConfCurrent::proxyTunnelSSL() ).toBool(); }
	bool PKCS12Disable() const override
	{ return s.value( "PKCS12Disable", digidoc::XmlConfCurrent::PKCS12Disable() ).toBool(); }
	std::string PKCS11Driver() const override
	{ return QString( qApp->applicationDirPath() + "/" + QFileInfo( PKCS11_MODULE ).fileName() ).toStdString(); }
	std::string TSLCache() const override
	{ return QStandardPaths::writableLocation(QStandardPaths::DataLocation).toStdString(); }
	bool TSLOnlineDigest() const override
	{ return s2.value( "TSLOnlineDigest", digidoc::XmlConfCurrent::TSLOnlineDigest() ).toBool(); }

	void setProxyHost( const std::string &host ) override
	{ s.setValueEx( "ProxyHost", QString::fromStdString( host ), QString() ); }
	void setProxyPort( const std::string &port ) override
	{ s.setValueEx( "ProxyPort", QString::fromStdString( port ), QString() ); }
	void setProxyUser( const std::string &user ) override
	{ s.setValueEx( "ProxyUser", QString::fromStdString( user ), QString() ); }
	void setProxyPass( const std::string &pass ) override
	{ s.setValueEx( "ProxyPass", QString::fromStdString( pass ), QString() ); }
	void setProxyTunnelSSL( bool enable ) override
	{ s.setValueEx( "ProxyTunnelSSL", enable, digidoc::XmlConfCurrent::proxyTunnelSSL() ); }
	void setPKCS12Cert( const std::string & ) override {}
	void setPKCS12Pass( const std::string & ) override {}
	void setPKCS12Disable( bool disable ) override
	{ s.setValueEx( "PKCS12Disable", disable, digidoc::XmlConfCurrent::PKCS12Disable() ); }
	void setTSLOnlineDigest( bool enable ) override
	{ s2.setValueEx( "TSLOnlineDigest", enable, digidoc::XmlConfCurrent::TSLOnlineDigest() ); }
#endif

	std::string TSUrl() const override { return value("TSA-URL", digidoc::XmlConfCurrent::TSUrl()); }
	std::string TSLUrl() const override { return value("TSL-URL", digidoc::XmlConfCurrent::TSLUrl()); }
	digidoc::X509Cert verifyServiceCert() const override
	{
		if(!obj.contains("SIVA-CERT"))
			return digidoc::XmlConfCurrent::verifyServiceCert();
		QByteArray cert = QByteArray::fromBase64(obj.value("SIVA-CERT").toString().toLatin1());
		return digidoc::X509Cert((const unsigned char*)cert.constData(), cert.size());
	}
	std::string verifyServiceUri() const override { return value("SIVA-URL", digidoc::XmlConfCurrent::verifyServiceUri()); }
	std::vector<digidoc::X509Cert> TSLCerts() const override
	{
		std::vector<digidoc::X509Cert> tslcerts;
		for(const QJsonValue &val: obj.value("TSL-CERTS").toArray())
		{
			QByteArray cert = QByteArray::fromBase64(val.toString().toLatin1());
			tslcerts.push_back(digidoc::X509Cert((const unsigned char*)cert.constData(), cert.size()));
		}
		return tslcerts.empty() ? digidoc::XmlConfCurrent::TSLCerts() : tslcerts;
	}
	std::string ocsp(const std::string &issuer) const override
	{
		QJsonObject ocspissuer = obj.value("OCSP-URL-ISSUER").toObject();
		for(QJsonObject::const_iterator i = ocspissuer.constBegin(); i != ocspissuer.constEnd(); ++i)
		{
			if(issuer == i.key().toStdString())
				return i.value().toString().toStdString();
		}
		return obj.value("OCSP-URL").toString(QString::fromStdString(digidoc::XmlConfCurrent::ocsp(issuer))).toStdString();
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
	void reload()
	{
		obj = Configuration::instance().object();
		QList<QSslCertificate> list;
		for(const QJsonValue &cert: obj.value("CERT-BUNDLE").toArray())
			list << QSslCertificate(QByteArray::fromBase64(cert.toString().toLatin1()), QSsl::Der);
		QSslSocket::setDefaultCaCertificates(list);
	}

	QNetworkProxy systemProxy() const
	{
		for(const QNetworkProxy &proxy: QNetworkProxyFactory::systemProxyForQuery())
		{
			if(proxy.type() == QNetworkProxy::HttpProxy)
				return proxy;
		}
		return QNetworkProxy();
	}

	std::string value(const QString &key, const std::string &defaultValue) const
	{
		return obj.value(key).toString(QString::fromStdString(defaultValue)).toStdString();
	}

	Settings s;
	Settings s2;
public:
	QJsonObject obj;
};

class ApplicationPrivate
{
public:
	QAction		*closeAction = nullptr, *newClientAction = nullptr, *newCryptoAction = nullptr;
	MacMenuBar	*bar = nullptr;
	QSigner		*signer = nullptr;
	QTranslator	appTranslator, commonTranslator, cryptoTranslator, qtTranslator;
	QString		lang;
	QTimer		lastWindowTimer;
	volatile bool ready = false;
	bool		macEvents = false;
};

Application::Application( int &argc, char **argv )
	: Common( argc, argv, APP, ":/images/digidoc_icon_128x128.png" )
	, d( new ApplicationPrivate )
{
	if(isCrashReport())
		return;

	QStringList args = arguments();
	args.removeFirst();
#ifndef Q_OS_MAC
	if( isRunning() )
	{
		sendMessage( args.join( "\", \"" ) );
		return;
	}
	connect( this, SIGNAL(messageReceived(QString)), SLOT(parseArgs(QString)) );
#endif

	QDesktopServices::setUrlHandler( "browse", this, "browse" );
	QDesktopServices::setUrlHandler( "mailto", this, "mailTo" );

	installTranslator( &d->appTranslator );
	installTranslator( &d->commonTranslator );
	installTranslator( &d->cryptoTranslator );
	installTranslator( &d->qtTranslator );
	loadTranslation( Settings::language() );

	// Actions
	d->closeAction = new QAction( tr("Close window"), this );
	d->closeAction->setShortcut( Qt::CTRL + Qt::Key_W );
	connect( d->closeAction, SIGNAL(triggered()), SLOT(closeWindow()) );
	d->newClientAction = new QAction( tr("New Client window"), this );
	d->newClientAction->setShortcut( Qt::CTRL + Qt::Key_N );
	connect( d->newClientAction, SIGNAL(triggered()), SLOT(showClient()) );

	setQuitOnLastWindowClosed( true );  // Oleg: this is needed to release application from memory (Windows)
	d->lastWindowTimer.setSingleShot(true);
	connect(&d->lastWindowTimer, &QTimer::timeout, [](){ if(topLevelWindows().isEmpty()) quit(); });
	connect(this, &Application::lastWindowClosed, [&](){ d->lastWindowTimer.start(10*1000); });

#if defined(Q_OS_MAC)
	d->bar = new MacMenuBar;
	d->bar->addAction( MacMenuBar::AboutAction, this, SLOT(showAbout()) );
	d->bar->fileMenu()->addAction( d->newClientAction );
	d->bar->fileMenu()->addAction( d->newCryptoAction );
	d->bar->fileMenu()->addAction( d->closeAction );
	d->bar->dockMenu()->addAction( d->newClientAction );
	d->bar->dockMenu()->addAction( d->newCryptoAction );
#endif

	QSigner::ApiType api = QSigner::PKCS11;
#ifdef Q_OS_WIN
	api = QSigner::ApiType(Settings(applicationName()).value("tokenBackend", QSigner::CNG).toUInt());
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
		d->signer = new QSigner( api, this );

		auto readVersion = [](const QString &path) -> uint {
			QFile f(path);
			if(!f.open(QFile::ReadOnly))
				return 0;
			QXmlStreamReader r(&f);
			while(!r.atEnd())
			{
				if(r.readNextStartElement() && r.name() == "TSLSequenceNumber")
				{
					r.readNext();
					return r.text().toUInt();
				}
			}
			return 0;
		};
		QString cache = confValue(TSLCache).toString();
		QDir().mkpath( cache );
		for(const QString &file: QDir(":/TSL/").entryList())
		{
			const QString target = cache + "/" + file;
			if(!QFile::exists(target) ||
				readVersion(":/TSL/" + file) > readVersion(target))
			{
				QFile::remove(target);
				QFile::copy(":/TSL/" + file, target);
				QFile::setPermissions(target, QFile::Permissions(0x6444));
			}
		}

		qRegisterMetaType<QEventLoop*>("QEventLoop*");
		digidoc::initialize( QString( "%1/%2 (%3)" )
			.arg( applicationName(), applicationVersion(), applicationOs() ).toUtf8().constData(),
			[](const digidoc::Exception *ex) {
				qDebug() << "TSL loading finished";
				if(ex) {
					QStringList causes;
					digidoc::Exception::ExceptionCode code = digidoc::Exception::General;
					DigiDoc::parseException(*ex, causes, code);
					QMetaObject::invokeMethod( qApp, "showWarning",
						Q_ARG(QString,tr("Failed to initalize.")), Q_ARG(QString,causes.join("\n")) );
				}
				qApp->d->ready = true;
				Q_EMIT qApp->TSLLoadingFinished();
			}
		);
	}
	catch( const digidoc::Exception &e )
	{
		showWarning( tr("Failed to initalize."), e );
		setQuitOnLastWindowClosed( true );
		return;
	}

	if( !args.isEmpty() || topLevelWindows().isEmpty() )
		parseArgs( args );
}

Application::~Application()
{
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
#endif
	w->addAction( d->closeAction );
	w->activateWindow();
	w->show();
	w->raise();
}

void Application::browse( const QUrl &url )
{
	QUrl u = url;
	u.setScheme( "file" );
#if defined(Q_OS_WIN)
	if( QProcess::startDetached( "explorer", QStringList() << "/select," <<
		QDir::toNativeSeparators( u.toLocalFile() ) ) )
		return;
#elif defined(Q_OS_MAC)
	if( QProcess::startDetached( "open", QStringList() << "-R" << u.toLocalFile() ) )
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
		case LDAP_HOST:
		case MobileID_URL:
		case MobileID_TEST_URL:
		case SiVaUrl:
		case TSLCerts:
		case TSLUrl:
		case TSLCache:
		case PKCS11Module: break;
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
		w->close(); // w->closeDoc();
	else
#endif
	if( QDialog *d = qobject_cast<QDialog*>(activeWindow()) )
		d->reject();
	else if( QWidget *w = qobject_cast<QWidget*>(activeWindow()) )
		w->close();
}

QVariant Application::confValue( ConfParameter parameter, const QVariant &value )
{
	DigidocConf *i = 0;
	try { i = static_cast<DigidocConf*>(digidoc::Conf::instance()); }
	catch( const digidoc::Exception & ) { return value; }

	QByteArray r;
	switch( parameter )
	{
	case LDAP_HOST: return i->obj.value("LDAP-HOST").toString("ldap.sk.ee:389");
	case MobileID_URL: return i->obj.value("MID-SIGN-URL").toString("https://digidocservice.sk.ee");
	case MobileID_TEST_URL: return i->obj.value("MID-SIGN-TEST-URL").toString("https://tsp.demo.sk.ee");
	case SiVaUrl: r = i->verifyServiceUri().c_str(); break;
	case PKCS11Module: r = i->PKCS11Driver().c_str(); break;
	case ProxyHost: r = i->proxyHost().c_str(); break;
	case ProxyPort: r = i->proxyPort().c_str(); break;
	case ProxyUser: r = i->proxyUser().c_str(); break;
	case ProxyPass: r = i->proxyPass().c_str(); break;
	case ProxySSL: return i->proxyTunnelSSL(); break;
	case PKCS12Cert: r = i->PKCS12Cert().c_str(); break;
	case PKCS12Pass: r = i->PKCS12Pass().c_str(); break;
	case PKCS12Disable: return i->PKCS12Disable();
	case TSLUrl: r = i->TSLUrl().c_str(); break;
	case TSLCache: r = i->TSLCache().c_str(); break;
	case TSLCerts:
	{
		QList<QSslCertificate> list;
		for(const digidoc::X509Cert &cert: i->TSLCerts())
		{
			std::vector<unsigned char> v = cert;
			if(!v.empty())
				list << QSslCertificate(QByteArray((const char*)&v[0], int(v.size())), QSsl::Der);
		}
		return QVariant::fromValue(list);
	}
	case TSLOnlineDigest: return i->TSLOnlineDigest();
	}
	return r.isEmpty() ? value.toString() : QString::fromUtf8( r );
}

void Application::diagnostics(QTextStream &s)
{
	s << "<br />TSL_URL: " << confValue(TSLUrl).toString()
		<< "<br />SIVA_URL: " << confValue(SiVaUrl).toString()
		<< "<br /><br /><b>" << tr("TSL signing certs") << ":</b>";
	for(const QSslCertificate &cert: confValue(TSLCerts).value<QList<QSslCertificate>>())
		s << "<br />" << cert.subjectInfo("CN").value(0);
}

bool Application::event( QEvent *e )
{
	switch( int(e->type()) )
	{
	case REOpenEvent::Type:
		if( !activeWindow() )
			parseArgs();
		return true;
	case QEvent::FileOpen:
		parseArgs( QStringList() << static_cast<QFileOpenEvent*>(e)->file() );
		return true;
#ifdef Q_OS_MAC
	// Load here because cocoa NSApplication overides events
	case QEvent::ApplicationActivate:
		if(!d->macEvents)
		{
			initMacEvents();
			d->macEvents = true;
		}
		return Common::event( e );
#endif
	default: return Common::event( e );
	}
}

void Application::loadTranslation( const QString &lang )
{
	if( d->lang == lang )
		return;
	Settings().setValue( "Main/Language", d->lang = lang );

	if( lang == "en" ) QLocale::setDefault( QLocale( QLocale::English, QLocale::UnitedKingdom ) );
	else if( lang == "ru" ) QLocale::setDefault( QLocale( QLocale::Russian, QLocale::RussianFederation ) );
	else QLocale::setDefault( QLocale( QLocale::Estonian, QLocale::Estonia ) );

	d->appTranslator.load( ":/translations/" + lang );
	d->commonTranslator.load( ":/translations/common_" + lang );
	d->cryptoTranslator.load( ":/translations/crypto_" + lang );
	d->qtTranslator.load( ":/translations/qt_" + lang );
	if( d->closeAction ) d->closeAction->setText( tr("Close window") );
	if( d->newClientAction ) d->newClientAction->setText( tr("New Client window") );
	if( d->newCryptoAction ) d->newCryptoAction->setText( tr("New Crypto window") );
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
		MapiFileDescW doc = { 0, 0, 0, 0, 0, 0 };
		doc.nPosition = -1;
		doc.lpszPathName = PWSTR(filePath.utf16());
		doc.lpszFileName = PWSTR(fileName.utf16());
		MapiMessageW message = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
		message.lpszSubject = PWSTR(subject.utf16());
		message.lpszNoteText = L"";
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
		MapiFileDesc doc = { 0, 0, 0, 0, 0, 0 };
		doc.nPosition = -1;
		doc.lpszPathName = LPSTR(filePath.constData());
		doc.lpszFileName = LPSTR(fileName.constData());
		MapiMessage message = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
		message.lpszSubject = LPSTR(subject.constData());
		message.lpszNoteText = "";
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
		p.start( "kreadconfig", QStringList()
			<< "--file" << "emaildefaults"
			<< "--group" << "PROFILE_Default"
			<< "--key" << "EmailClient" );
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
		status = p.startDetached( thunderbird, QStringList() << "-compose"
			<< QString( "subject='%1',attachment='%2'" )
				.arg( q.queryItemValue( "subject" ) )
				.arg( QUrl::fromLocalFile( q.queryItemValue( "attachment" ) ).toString() ) );
	}
	else
	{
		status = p.startDetached( "xdg-email", QStringList()
			<< "--subject" << q.queryItemValue( "subject" )
			<< "--attach" << q.queryItemValue( "attachment" ) );
	}
	if( status )
		return;
#endif
	QDesktopServices::openUrl( url );
}
#endif

bool Application::notify( QObject *o, QEvent *e )
{
	try
	{
		return QApplication::notify( o, e );
	}
	catch( const digidoc::Exception &e )
	{
		showWarning( tr("Caught exception!"), e );
	}
	catch(...)
	{
		showWarning( tr("Caught exception!") );
	}

	return false;
}

void Application::parseArgs( const QString &msg )
{
	QStringList params;
	for(const QString &param: msg.split("\", \"", QString::SkipEmptyParts))
	{
		QUrl url( param, QUrl::StrictMode );
		params << (param != "-crypto" && !url.toLocalFile().isEmpty() ? url.toLocalFile() : param);
	}
	parseArgs( params );
}

void Application::parseArgs( const QStringList &args )
{
	bool crypto = args.contains("-crypto");
	QStringList params = args;
	params.removeAll("-crypto");
	params.removeAll("-capi");
	params.removeAll("-cng");
	params.removeAll("-pkcs11");
	params.removeAll("-noNativeFileDialog");

	QString suffix = QFileInfo( params.value( 0 ) ).suffix();
	showClient( params );
}

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
		case LDAP_HOST:
		case MobileID_URL:
		case MobileID_TEST_URL:
		case SiVaUrl:
		case TSLCerts:
		case TSLUrl:
		case TSLCache:
		case PKCS11Module: break;
		}
	}
	catch( const digidoc::Exception &e )
	{
		showWarning(tr("Caught exception!"), e);
	}
}

void Application::showAbout()
{
	AboutDialog *a = new AboutDialog( activeWindow() );
	a->addAction( d->closeAction );
	a->open();
}

void Application::showClient( const QStringList &params )
{
	QWidget *w = 0;
	for(QWidget *m: qApp->topLevelWidgets())
	{
		MainWindow *main = qobject_cast<MainWindow*>(m);
		if( main )
		{
			w = main;
			break;
		}
	}
	if( !w )
	{
		Settings settings;
		if( settings.value("showIntro", true).toBool() )
		{
			FirstRun dlg;
			dlg.exec();
			settings.setValue( "showIntro", false ); 
		}
	
		w = new MainWindow();
	}
	if( !params.isEmpty() )
		QMetaObject::invokeMethod( w, "open", Q_ARG(QStringList,params) );
	activate( w );
}

void Application::showTSLWarning(QEventLoop *e)
{
	e->exit( QMessageBox::information(
		qApp->activeWindow(), Application::tr("DigiDoc4 Client"), Application::tr(
		"The renewal of Trust Service status List, used for digital signature validation, has failed. "
		"Please check your internet connection and make sure you have the latest ID-software version "
		"installed. An expired Trust Service List (TSL) will be used for signature validation. "
		"<a href=\"http://www.id.ee/?id=37012\">Additional information</a>") ) );
}

void Application::showWarning( const QString &msg, const digidoc::Exception &e )
{
	QStringList causes;
	digidoc::Exception::ExceptionCode code = digidoc::Exception::General;
	DigiDoc::parseException(e, causes, code);
	QMessageBox d(QMessageBox::Warning, tr("DigiDoc4 client"), msg, QMessageBox::Close, activeWindow());
	d.setWindowModality(Qt::WindowModal);
	d.setDetailedText(causes.join("\n"));
	d.exec();
}

void Application::showWarning( const QString &msg, const QString &details )
{
	QMessageBox d( QMessageBox::Warning, tr("DigiDoc4 client"), msg, QMessageBox::Close, activeWindow() );
	d.setWindowModality( Qt::WindowModal );
	d.setDetailedText(details);
	d.exec();
}

QSigner* Application::signer() const { return d->signer; }

void Application::waitForTSL( const QString &file )
{
	if( !QStringList({"asice", "sce", "bdoc", "asics", "scs"}).contains(QFileInfo(file).suffix(), Qt::CaseInsensitive) )
		return;

	if( d->ready )
		return;

	QProgressDialog p( tr("Loading TSL lists"), QString(), 0, 0, qApp->activeWindow() );
	p.setWindowFlags( (p.windowFlags() | Qt::CustomizeWindowHint) & ~Qt::WindowCloseButtonHint );
	if( QProgressBar *bar = p.findChild<QProgressBar*>() )
		bar->setTextVisible( false );
	p.setMinimumWidth( 300 );
	p.setRange( 0, 100 );
	p.open();
	QTimer t;
	connect( &t, &QTimer::timeout, [&](){
		p.setValue( p.value() + 1 );
		if( p.value() == p.maximum() )
			p.reset();
		t.start( 100 );
	});
	t.start( 100 );
	QEventLoop e;
	connect(this, &Application::TSLLoadingFinished, &e, &QEventLoop::quit);
	if( !d->ready )
		e.exec();
	t.stop();
}

DdCliApplication::DdCliApplication( int &argc, char **argv )
	: CliApplication( argc, argv, APP )
{
}

void DdCliApplication::diagnostics(QTextStream &s) const
{
	digidoc::Conf::init( new DigidocConf );

	s << "<br />TSL_URL: " << Application::confValue(Application::TSLUrl).toString();
	s << "<br /><br /><b>" << "TSL signing certs:</b>";
	for(const QSslCertificate &cert: Application::confValue(Application::TSLCerts).value<QList<QSslCertificate>>())
		s << "<br />" << cert.subjectInfo("CN").value(0);
}

void Application::showSettings( int page, const QString &path )
{
/*
	SettingsDialog *s = new SettingsDialog( page, activeWindow() );
	s->addAction( d->closeAction );
	s->open();
	if( !path.isEmpty() )
		s->activateAccessCert( path );
 */
}
