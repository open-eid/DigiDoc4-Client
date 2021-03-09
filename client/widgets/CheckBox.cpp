/*
 * QDigiDoc4
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

#include "CheckBox.h"

#include <QBrush>
#include <QPaintEvent>
#include <QPainter>
#include <QPainterPath>
#include <QStyleOptionButton>

CheckBox::CheckBox(QWidget *parent)
	: QCheckBox(parent)
{}

void CheckBox::paintEvent(QPaintEvent *event)
{
	QCheckBox::paintEvent(event);
	QStyleOptionButton opt;
	initStyleOption(&opt);
	QRect rect = style()->subElementRect(QStyle::SE_RadioButtonIndicator, &opt, this);
	rect.adjust(1, 1, -1, -1);
	QPainter painter(this);
	painter.setRenderHint(QPainter::Antialiasing);
	painter.setRenderHint(QPainter::HighQualityAntialiasing);
	painter.fillRect(rect, Qt::white);
	painter.setPen(QStringLiteral("#AAADAD"));
	painter.drawRoundedRect(rect, 2.0, 2.0);
	if (isChecked())
	{
		rect.adjust(3, 3, -3, -3);
		painter.setPen(QPen(QColor(QStringLiteral("#0C5AA6")), 2, Qt::SolidLine, Qt::RoundCap));
		QPainterPath path(QPointF(rect.left(), rect.center().y()));
		path.lineTo(rect.center().x(), rect.bottom());
		path.lineTo(rect.right(), rect.top());
		painter.drawPath(path);
	}
}
