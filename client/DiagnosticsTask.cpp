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
#include "DiagnosticsTask.h"

#include <iostream>
#include <cstdio>

#include <QFile>
#include <QTextDocument>
#include <QTextStream>

DiagnosticsTask::DiagnosticsTask(QObject *parent, QString outFile)
	: QObject(parent), outFile(std::move(outFile))
{
}

void DiagnosticsTask::run()
{
	QObject::connect( &worker, &Diagnostics::update, this, &DiagnosticsTask::insertHtml );
	worker.run();
	complete();

	if( logDiagnostics() )
	{
		emit finished();
	}
	else
	{
		emit failed();
	}
}

bool DiagnosticsTask::logDiagnostics()
{
	QFile file( outFile );
	bool isOpened = false;

	if( outFile.isEmpty() )
	{
		isOpened = file.open( stdout, QIODevice::WriteOnly );
	}
	else
	{
		isOpened = file.open( QIODevice::WriteOnly|QIODevice::Text );
	}

	if ( isOpened )
	{
		QTextStream out( &file );
		out << data;
		out.flush();
	}
	else
	{
		std::cerr << outFile.toStdString() << ": " << file.errorString().toStdString() << std::endl;
	}

	return isOpened;
}

void DiagnosticsTask::insertHtml( const QString &text )
{
	html << text;
}

void DiagnosticsTask::complete()
{
	QTextDocument doc;
	doc.setHtml(html.join(QString()));
	data = doc.toPlainText();
}
