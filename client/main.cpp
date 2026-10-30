/*
 * QDigiDocClient
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

#include "Application.h"

#include "DiagnosticsTask.h"

#include <QtCore/QTimer>
#include <QtCore/QRegularExpression>

int main( int argc, char *argv[] )
{
#ifdef Q_OS_LINUX
	// On GNOME Qt picks the GTK3 platform theme, whose file chooser hard-codes
	// gtk_file_chooser_set_local_only(true). That hides every location without a
	// native path, so gvfs network mounts (NAS over SFTP/SMB, added in Files) are
	// missing from the save dialog even though they are mounted and writable.
	// The xdg-desktop-portal chooser is GTK4, which dropped local_only entirely,
	// so it lists them. Qt never selects that theme on its own, hence the override.
	//
	// Requires qt6-xdgdesktopportal-platformtheme (a Recommends of this package).
	// If it is missing Qt falls through to gtk3, i.e. the previous behaviour, so
	// tell users who cannot see a network location in the save dialog to install
	// that package. Setting QT_QPA_PLATFORMTHEME yourself overrides this.
	if(qEnvironmentVariableIsEmpty("QT_QPA_PLATFORMTHEME"))
		qputenv("QT_QPA_PLATFORMTHEME", "xdgdesktopportal");
#endif

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
