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

#include <QtCore/QObject>
#include <QtCore/QRunnable>

class QTextStream;

class Diagnostics: public QObject, public QRunnable
{
	Q_OBJECT
public:
	Diagnostics();

	void run() override;

signals:
	void update( const QString &data );

private:
	void generalInfo(QTextStream &s) const;
	QStringList packages(const QStringList &names, bool withName = true);

#if defined(Q_OS_WIN) || defined(Q_OS_LINUX)
	QString packageName(const QString &name, const QString &ver, bool withName)
	{
		return withName ? name + " (" + ver + ")" : ver;
	}
#endif
};
