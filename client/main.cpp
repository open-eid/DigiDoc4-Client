// SPDX-FileCopyrightText: Estonian Information System Authority
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "Application.h"

#include "DiagnosticsTask.h"

#include <QtCore/QTimer>
#include <QtCore/QRegularExpression>

int main( int argc, char *argv[] )
{
	for(int i = 1; i < argc; ++i)
	{
		QString parameter(argv[i]);
		if(parameter.startsWith(QStringLiteral("-diag")))
		{
			QCoreApplication qtApp( argc, argv );
			qtApp.setApplicationName(QStringLiteral("qdigidoc4"));
			qtApp.setApplicationVersion(QStringLiteral(VERSION_STR));
			qtApp.setOrganizationDomain(QStringLiteral("ria.ee"));
			qtApp.setOrganizationName(QStringLiteral("RIA"));

			Application::initDiagnosticConf();
			DiagnosticsTask task(&qtApp, parameter.remove(QStringLiteral("-diag")).remove(QRegularExpression(QStringLiteral("^[:]*"))));
			QObject::connect(&task, &DiagnosticsTask::finished, &qtApp, &QCoreApplication::quit);
			QObject::connect(&task, &DiagnosticsTask::failed, &qtApp, [] { QCoreApplication::exit(1); });

			QTimer::singleShot(0, &task, &DiagnosticsTask::run);
			return qtApp.exec();
		}
	}

	return Application( argc, argv ).run();
}
