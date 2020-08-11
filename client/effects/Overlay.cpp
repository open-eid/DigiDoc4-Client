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

#include "Overlay.h"

#include <QGraphicsBlurEffect>
#include <QPainter>
#include <QPaintEvent>

Overlay::Overlay(QWidget *runner, QWidget *parent)
	: Overlay(parent)
{
	connect(runner, &QWidget::destroyed, this, &Overlay::deleteLater);
	show();
}

Overlay::Overlay(QWidget *parent): QWidget(parent)
{
	setPalette(Qt::transparent);
	setAttribute(Qt::WA_TransparentForMouseEvents);
	if(parent)
	{
		setMinimumSize(parent->width(), parent->height());
		parent->setGraphicsEffect(new QGraphicsBlurEffect);
	}
}

Overlay::~Overlay()
{
	if(parentWidget())
		parentWidget()->setGraphicsEffect(nullptr);
}

void Overlay::paintEvent(QPaintEvent * /*event*/)
{
	if(parentWidget())
		setMinimumSize(parentWidget()->width(), parentWidget()->height());
	QPainter painter( this );
	painter.setRenderHint( QPainter::Antialiasing );
	// Opacity 90%
	painter.setBrush(QBrush(QColor(0x04, 0x1E, 0x42, int(double(0xff) * 0.9))));
	painter.setPen( Qt::NoPen );
	painter.drawRect( rect() );
}
