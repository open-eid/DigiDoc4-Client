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

#include "LineEdit.h"

#include <QtGui/QPainter>

LineEdit::LineEdit(QWidget *parent)
	: QLineEdit(parent)
{}

void LineEdit::paintEvent(QPaintEvent *event)
{
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
	// Workaround QTBUG-92199
	if(text().isEmpty() && (!placeholderText().isEmpty() || !placeholder.isEmpty()))
	{
		if(!placeholderText().isEmpty())
		{
			placeholder = placeholderText();
			setPlaceholderText(QString());
		}
		QLineEdit::paintEvent(event);

		QPainter p(this);
		QColor color = palette().color(QPalette::PlaceholderText);
		color.setAlpha(63);
		p.setPen(color);
		QFontMetrics fm = fontMetrics();
		int minLB = qMax(0, -fm.minLeftBearing());
		QRect lineRect = this->rect();
		QRect ph = lineRect.adjusted(minLB + 3, 0, 0, 0);
		QString elidedText = fm.elidedText(placeholder, Qt::ElideRight, ph.width());
		p.drawText(ph, Qt::AlignVCenter, elidedText);
		return;
	}
#endif
	QLineEdit::paintEvent(event);
}
