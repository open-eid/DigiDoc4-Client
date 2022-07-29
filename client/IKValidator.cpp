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

#include "IKValidator.h"

#include <QtCore/QDate>
#include <QtCore/QStringList>

QDate IKValidator::birthDate(const QString &ik)
{
	if(ik.size() != 11) return {};

	quint16 year = 0;
	switch(ik.left(1).toUInt())
	{
	case 1: case 2: year = 1800; break;
	case 3: case 4: year = 1900; break;
	case 5: case 6: year = 2000; break;
	case 7: case 8: year = 2100; break;
	default: return {};
	}

	QDate date(
		ik.mid(1, 2).toInt() + year,
		ik.mid(3, 2).toInt(),
		ik.mid(5, 2).toInt());
	return date.isValid() ? date : QDate();
}

bool IKValidator::isValid(const QString &ik)
{
	if(ik.size() != 11)
		return false;

	// Mobile-ID test IK-s
	static const QStringList list {
		QStringLiteral("14212128020"),
		QStringLiteral("14212128021"),
		QStringLiteral("14212128022"),
		QStringLiteral("14212128023"),
		QStringLiteral("14212128024"),
		QStringLiteral("14212128025"),
		QStringLiteral("14212128026"),
		QStringLiteral("14212128027"),
		QStringLiteral("38002240211"),
		QStringLiteral("14212128029"),
	};
	if(list.contains(ik))
		return true;

	// Validate date
	if(birthDate(ik).isNull())
		return false;

	// Validate checksum
	int sum1 = 0, sum2 = 0;
	for(int i = 0, pos1 = 1, pos2 = 3; i < 10; ++i)
	{
		sum1 += ik.mid(i, 1).toInt() * pos1;
		sum2 += ik.mid(i, 1).toInt() * pos2;
		pos1 = pos1 == 9 ? 1 : pos1 + 1;
		pos2 = pos2 == 9 ? 1 : pos2 + 1;
	}

	int result;
	if((result = sum1 % 11) >= 10 &&
		(result = sum2 % 11) >= 10)
		result = 0;

	return ik.right( 1 ).toInt() == result;
}



NumberValidator::NumberValidator(QObject *parent)
	: QValidator(parent)
{}

NumberValidator::State NumberValidator::validate(QString &input, int & /*pos*/) const
{
	QString out;
	QRegularExpression rx(QStringLiteral("(\\d+)"));
	QRegularExpressionMatch match = rx.match(input);
	for(int i = 0; i < match.lastCapturedIndex(); ++i)
		out += match.captured(i);
	input = out;
	return Acceptable;
}
