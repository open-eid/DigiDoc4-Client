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

#pragma once

#include <common/Common.h>

#include <QtCore/QStringList>
#include <QtCore/QVariant>

#if defined(qApp)
#undef qApp
#endif
#define qApp (static_cast<Application*>(QCoreApplication::instance()))

namespace digidoc { class Exception; }
class QAction;
class QSigner;
class Application final: public Common
{
	Q_OBJECT

public:
	enum ConfParameter
	{
		SiVaUrl,
		ProxyHost,
		ProxyPort,
		ProxyUser,
		ProxyPass,
		ProxySSL,
		PKCS12Cert,
		PKCS12Pass,
		PKCS12Disable,
		TSAUrl,
		TSLUrl,
		TSLCerts,
		TSLCache,
		TSLOnlineDigest
	};

	explicit Application( int &argc, char **argv );
	~Application() final;

#ifdef Q_OS_WIN
	void addTempFile(const QString &file);
#endif
	void loadTranslation( const QString &lang );
	QWidget* mainWindow();
	bool notify(QObject *object, QEvent *event ) final;
	void openHelp();
	QSigner* signer() const;
	int run();
	void waitForTSL( const QString &file );

	static void initDiagnosticConf();
	static uint readTSLVersion(const QString &path);
	static void addRecent( const QString &file );
	static QVariant confValue(ConfParameter parameter, const QVariant &value = {});
	static void clearConfValue( ConfParameter parameter );
	static void setConfValue( ConfParameter parameter, const QVariant &value );
#if defined(Q_OS_MAC)
	static QString groupContainer();
#endif

public Q_SLOTS:
	void showAbout();
	void showSettings();
	void showClient(const QStringList &params = {}, bool crypto = false, bool sign = false, bool newWindow = false);
	void showWarning(const QString &msg, const QString &details = {});

private Q_SLOTS:
	void browse( const QUrl &url );
	void closeWindow();
	void mailTo( const QUrl &url );
	void parseArgs(const QString &msg = {});
	void parseArgs( const QStringList &args );
	void showTSLWarning( QEventLoop *e );

Q_SIGNALS:
	void TSLLoadingFinished();

private:
	void activate( QWidget *w );
	bool event(QEvent *event) final;
	void migrateSettings();
	static void showWarning(const QString &msg, const digidoc::Exception &e);
	QWidget* uniqueRoot();

#if defined(Q_OS_MAC)
	void initMacEvents();
	void deinitMacEvents();
#endif

	static const QStringList CONTAINER_EXT;
	class Private;
	Private *d;
};

class REOpenEvent: public QEvent
{
public:
	enum { Type = QEvent::User + 1 };
	REOpenEvent(): QEvent( QEvent::Type(Type) ) {}
};
