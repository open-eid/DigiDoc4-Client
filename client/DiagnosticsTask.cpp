// SPDX-FileCopyrightText: Estonian Information System Authority
// SPDX-License-Identifier: LGPL-2.1-or-later

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
