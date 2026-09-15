// SPDX-FileCopyrightText: Estonian Information System Authority
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "Diagnostics.h"
#include <QObject>
#include <QString>
#include <QStringList>

class DiagnosticsTask: public QObject
{
	Q_OBJECT
public:
	DiagnosticsTask(QObject *parent, QString outFile);
	void complete();

public slots:
	void run();
	void insertHtml( const QString &text );

signals:
	void finished();
	void failed();

private:
	QStringList html;
	QString data;
	QString outFile;
	Diagnostics worker;

	bool logDiagnostics();
};
