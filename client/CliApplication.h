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

#pragma once

#include <QString>
#include <QTextStream>

class CliApplication: public QObject
{
	Q_OBJECT
public:
	CliApplication( int &argc, char **argv, const QString &appName );
	CliApplication( int &argc, char **argv, const QString &appName, const QString &outFile );

	bool isDiagnosticRun();
	int run() const;

public slots:
	void exit() const;


protected:
	// Override method to add an application-specific message
	// to the diagnostics after the "URLs" block
	virtual void diagnostics( QTextStream &s ) const;

private:
	int &argc;
	char **argv;
	QString appName;
	QString outFile;
};
