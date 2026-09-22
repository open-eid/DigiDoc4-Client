// SPDX-FileCopyrightText: Estonian Information System Authority
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include <QtCore/QDateTime>

class DateTime final: public QDateTime
{
public:
	DateTime(const QDateTime &other);

	QString formatDate(const QString &format) const;
	QString toStringZ(const QString &format) const;

private:
	qint64 timeDiff() const;
};
