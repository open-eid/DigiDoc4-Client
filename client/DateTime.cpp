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

#include "DateTime.h"

#include <QtCore/QLocale>

DateTime::DateTime( const QDateTime &other )
	: QDateTime( other )
{}

QString DateTime::formatDate( const QString &format ) const
{
	int pos = format.indexOf(QStringLiteral("MMMM"));
	if(pos == -1)
		return toString(format);
	QString d = toString(QString(format).remove(pos, 4));
	return d.insert(pos, QLocale().monthName(date().month()));
}

qint64 DateTime::timeDiff() const
{
	if(!isValid() || timeSpec() == Qt::UTC)
		return 0;
	QDateTime utc = toUTC();
	utc.setTimeSpec(Qt::LocalTime);
	return utc.secsTo(toLocalTime());
}

QString DateTime::toStringZ( const QString &format ) const
{
	if(!isValid())
		return {};
	qint64 diffsec = timeDiff();
	return toString(format) + QStringLiteral(" %1%2").arg(diffsec >= 0 ? '+' : '-')
		.arg(QTime(0, 0).addSecs(diffsec >= 0 ? diffsec : -diffsec).toString(QStringLiteral("hh:mm")));
}
