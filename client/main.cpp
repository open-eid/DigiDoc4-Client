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

#ifdef Q_OS_WIN32
#include <QtCore/QDebug>
#include <QtCore/qt_windows.h>
#endif

int main( int argc, char *argv[] )
{
#if QT_VERSION > QT_VERSION_CHECK(5, 6, 0)
	QCoreApplication::setAttribute(Qt::AA_UseHighDpiPixmaps, true);
#ifdef Q_OS_WIN32
	SetProcessDPIAware();
	HDC screen = GetDC(0);
	qreal dpix = GetDeviceCaps(screen, LOGPIXELSX);
	qreal dpiy = GetDeviceCaps(screen, LOGPIXELSY);
	qreal scale = dpiy / qreal(96);
	qputenv("QT_SCALE_FACTOR", QByteArray::number(scale));
	ReleaseDC(NULL, screen);
	qDebug() << "Current DPI x: " << dpix << " y: " << dpiy << " setting scale:" << scale;
#else
	QCoreApplication::setAttribute(Qt::AA_EnableHighDpiScaling, true);
#endif
#endif

	DdCliApplication cliApp( argc, argv );
	if( cliApp.isDiagnosticRun() )
	{
		return cliApp.run();
	}

	return Application( argc, argv ).run();
}
