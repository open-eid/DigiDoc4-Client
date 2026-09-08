// SPDX-FileCopyrightText: Estonian Information System Authority
// SPDX-License-Identifier: LGPL-2.1-or-later

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
