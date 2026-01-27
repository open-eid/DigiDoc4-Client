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
#include <QtWidgets/QStyleOptionButton>

LineEdit::LineEdit(QWidget *parent)
	: QLineEdit(parent)
{}

void LineEdit::paintEvent(QPaintEvent *event)
{
#if QT_VERSION < QT_VERSION_CHECK(6, 4, 1)
	// Workaround QTBUG-92199
	if(text().isEmpty() && (!placeholderText().isEmpty() || !placeholder.isEmpty()))
	{
		if(!placeholderText().isEmpty())
		{
			placeholder = placeholderText();
			setPlaceholderText({});
		}
		QLineEdit::paintEvent(event);

		QStyleOptionFrame opt;
		initStyleOption(&opt);
		QRect lineRect = style()->subElementRect(QStyle::SE_LineEditContents, &opt, this);
		lineRect = lineRect.adjusted(3, 0, 0, 0);
		QColor color = palette().color(QPalette::PlaceholderText);
		color.setAlpha(63);
		QPainter p(this);
		p.setClipRect(lineRect);
		p.setPen(color);
		p.drawText(lineRect, Qt::AlignVCenter,
			fontMetrics().elidedText(placeholder, Qt::ElideRight, lineRect.width()));
		return;
	} else
#endif
	QLineEdit::paintEvent(event);
	if(_label == QStringLiteral("error"))
	{
		QStyleOptionFrame opt;
		initStyleOption(&opt);
		QRect rect = style()->subElementRect(QStyle::SE_LineEditContents, &opt, this);
		auto adjust = (rect.height() - 17) / 2;
		rect.adjust(rect.width() - adjust - 17, adjust, -adjust, -adjust);
		QPainter p(this);
		p.drawImage(rect, QImage(QStringLiteral(":/images/icon_error.svg")));
	}
}

QString LineEdit::label() const
{
	return _label;
}

void LineEdit::setLabel(QString label)
{
	if (label == _label)
		return;
	_label = std::move(label);
	parentWidget()->style()->unpolish(this);
	parentWidget()->style()->polish(this);
}
