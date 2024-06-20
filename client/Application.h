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
class Configuration;
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
		TSAUrl,
		TSLUrl,
		TSLCerts,
		TSLCache,
	};

	explicit Application( int &argc, char **argv );
	~Application() final;

#ifdef Q_OS_WIN
	void addTempFile(const QString &file);
#endif
	Configuration *conf();
	void loadTranslation( const QString &lang );
	bool notify(QObject *object, QEvent *event ) final;
	QSigner* signer() const;
	int run();
	void waitForTSL( const QString &file );

	static void addRecent( const QString &file );
	template<class T>
	static QJsonValue confValue(const T &key);
	static QVariant confValue(ConfParameter parameter, const QVariant &value = {});
	static void initDiagnosticConf();
	static QWidget* mainWindow();
	static void openHelp();
	static uint readTSLVersion(const QString &path);
	static void setConfValue( ConfParameter parameter, const QVariant &value );
	static void showClient(const QStringList &params = {}, bool crypto = false, bool sign = false, bool newWindow = false);
	static void updateTSLCache(const QDateTime &tslTime);

private Q_SLOTS:
	static void browse(const QUrl &url);
	static void mailTo(const QUrl &url);

Q_SIGNALS:
	void TSLLoadingFinished();

private:
	bool event(QEvent *event) final;
	static void closeWindow();
	static void parseArgs(const QString &msg = {});
	static void parseArgs(QStringList args);
	static void showWarning(const QString &msg, const digidoc::Exception &e);
#if defined(Q_OS_MAC)
	static void initMacEvents();
	static void deinitMacEvents();
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

extern template QJsonValue Application::confValue<QString>(const QString &key);
extern template QJsonValue Application::confValue<QLatin1String>(const QLatin1String &key);
