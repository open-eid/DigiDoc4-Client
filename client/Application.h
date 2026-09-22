// SPDX-FileCopyrightText: Estonian Information System Authority
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include <QtCore/QtGlobal>

#ifdef Q_OS_MAC
#include <QtWidgets/QApplication>
using BaseApplication = QApplication;
#else
#include "qtsingleapplication/src/QtSingleApplication"
using BaseApplication = QtSingleApplication;
#endif

#if defined(qApp)
#undef qApp
#endif
#define qApp (static_cast<Application*>(QCoreApplication::instance()))

namespace digidoc { class Exception; }
class Configuration;
class QAction;
class QSigner;
class Application final: public BaseApplication
{
	Q_OBJECT

public:
	enum ConfParameter : quint8
	{
		SiVaUrl,
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
	static void showClient(QStringList params = {}, bool crypto = false, bool sign = false, bool newWindow = false);
	static void updateTSLCache(const QDateTime &tslTime);
#if defined(Q_OS_MAC)
	static QString groupContainerPath();
#endif

private Q_SLOTS:
	static void browse(const QUrl &url);
	static void mailTo(const QUrl &url);

Q_SIGNALS:
	void TSLLoadingFinished();

private:
	bool event(QEvent *event) final;
	static void closeWindow();
	static void msgHandler(QtMsgType type, const QMessageLogContext &ctx, const QString &msg);
	static void parseArgs(const QString &msg = {});
	static void parseArgs(QStringList args);
	static void showWarning(const QString &title, const digidoc::Exception &e);
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
	enum : quint16 { Type = QEvent::User + 1 };
	REOpenEvent(): QEvent( QEvent::Type(Type) ) {}
};

extern template QJsonValue Application::confValue<QString>(const QString &key);
extern template QJsonValue Application::confValue<QLatin1String>(const QLatin1String &key);
