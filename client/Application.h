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

#include "CliApplication.h"
#include <common/Common.h>

#include <QtCore/QStringList>
#include <QtCore/QVariant>

#if defined(qApp)
#undef qApp
#endif
#define qApp (static_cast<Application*>(QCoreApplication::instance()))

namespace digidoc { class Exception; }
class QAction;
class QSmartCard;
class QSigner;
class Application: public Common
{
	Q_OBJECT

public:
	enum ConfParameter
	{
		LDAP_PERSON_URL,
		LDAP_CORP_URL,
		MobileID_URL,
		MobileID_TEST_URL,
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
	bool initialized();
	void loadTranslation( const QString &lang );
	bool notify( QObject *o, QEvent *e ) override;
	QSigner* signer() const;
	QSmartCard* smartcard() const;
	int run();
	void waitForTSL( const QString &file );

	static uint readTSLVersion(const QString &path);
	static void addRecent( const QString &file );
	static QVariant confValue( ConfParameter parameter, const QVariant &value = QVariant() );
	static void clearConfValue( ConfParameter parameter );
	static void setConfValue( ConfParameter parameter, const QVariant &value );

public Q_SLOTS:
	void showAbout();
	void showSettings();
	void showClient(const QStringList &params = QStringList(), bool crypto = false, bool sign = false);
	void showWarning(const QString &msg, const QString &details = QString());

private Q_SLOTS:
	void browse( const QUrl &url );
	void closeWindow();
	void mailTo( const QUrl &url );
	void parseArgs( const QString &msg = QString() );
	void parseArgs( const QStringList &args );
	void showTSLWarning( QEventLoop *e );

Q_SIGNALS:
	void TSLLoadingFinished();

private:
	void activate( QWidget *w );
	void diagnostics(QTextStream &s) override;
	bool event( QEvent *e ) override;
	QWidget* mainWindow();
	void migrateSettings();
	static void showWarning(const QString &msg, const digidoc::Exception &e);
	QWidget* uniqueRoot();

#if defined(Q_OS_MAC)
	void initMacEvents();
	void deinitMacEvents();
#endif

	class Private;
	Private *d;
};

class REOpenEvent: public QEvent
{
public:
	enum { Type = QEvent::User + 1 };
	REOpenEvent(): QEvent( QEvent::Type(Type) ) {}
};

class DdCliApplication: public CliApplication
{
public:
	DdCliApplication( int &argc, char **argv );

protected:
	virtual void diagnostics( QTextStream &s ) const;
};
