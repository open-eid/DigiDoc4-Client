/*
 * QEstEidCommon
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
#include "CliApplication.h"
#include "DiagnosticsTask.h"

#include <QtCore/QObject>
#include <QtCore/QCoreApplication>
#include <QtCore/QTimer>


CliApplication::CliApplication( int &argc, char **argv, const QString &appName )
	: CliApplication( argc, argv, appName, QString() )
{
}

CliApplication::CliApplication( int &argc, char **argv, const QString &appName, const QString &outFile )
	: argc(argc), argv(argv), appName(appName), outFile(outFile)
{
}

bool CliApplication::isDiagnosticRun()
{
	for( int i = 1; i < argc; ++i )
	{
		auto parameter = QString( argv[i] );
		if( parameter.startsWith("-diag") )
		{
			outFile = parameter.remove("-diag").remove(QRegExp("^[:]*"));
			return true;
		}
	}

	return false;
}

int CliApplication::run() const
{
	QCoreApplication qtApp( argc, argv );

	qtApp.setApplicationName( appName );
	qtApp.setApplicationVersion( QString( "%1.%2.%3.%4" )
		.arg( MAJOR_VER ).arg( MINOR_VER ).arg( RELEASE_VER ).arg( BUILD_VER ) );
	qtApp.setOrganizationDomain( "ria.ee" );
	qtApp.setOrganizationName("RIA");

	QString appInfo;
	QTextStream s( &appInfo );
	diagnostics(s);

	DiagnosticsTask task( &qtApp, appInfo, outFile );
	QObject::connect( &task, SIGNAL(finished()), &qtApp, SLOT(quit()));
	QObject::connect( &task, SIGNAL(failed()), this, SLOT(exit()));

	QTimer::singleShot( 0, &task, SLOT(run()) );
	return qtApp.exec();
}

void CliApplication::diagnostics( QTextStream & ) const
{
}

void CliApplication::exit() const
{
	QCoreApplication::exit(1);
}