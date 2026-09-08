// SPDX-FileCopyrightText: Estonian Information System Authority
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include <QtCore/QObject>
#include <QtCore/QRunnable>

class QTextStream;

class Diagnostics: public QObject, public QRunnable
{
	Q_OBJECT
public:
	void run() override;

signals:
	void update( const QString &data );

private:
	static void generalInfo(QTextStream &s) ;
	QStringList packages(const QStringList &names, bool withName = true);

#if defined(Q_OS_WIN) || defined(Q_OS_LINUX)
	QString packageName(const QString &name, const QString &ver, bool withName)
	{
		return withName ? name + " (" + ver + ")" : ver;
	}
#endif
};
