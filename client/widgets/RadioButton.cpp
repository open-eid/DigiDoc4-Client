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

#include "RadioButton.h"

#include <QBrush>
#include <QPaintEvent>
#include <QPainter>

RadioButton::RadioButton(QWidget *parent)
	: QRadioButton(parent)
{}

void RadioButton::paintEvent(QPaintEvent *event)
{
	QRadioButton::paintEvent(event);
	QRect rect(0, 0, 14, 14);
	rect.moveCenter(QPoint(rect.center().x() + 1, event->rect().center().y()));
	QPainter painter(this);
	painter.setRenderHint(QPainter::Antialiasing);
	painter.setRenderHint(QPainter::HighQualityAntialiasing);
	painter.fillRect(rect, Qt::white);
	painter.setPen(QStringLiteral("#AAADAD"));
	painter.drawEllipse(rect);
	if (isChecked())
	{
		rect.adjust(3, 3, -3, -3);
		painter.setPen(QStringLiteral("#0C5AA6"));
		painter.setBrush(painter.pen().color());
		painter.drawEllipse(rect);
	}
}
