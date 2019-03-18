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

#include "ButtonHoverFilter.h"

#include <QtCore/QEvent>
#include <QtWidgets/QAbstractButton>

ButtonHoverFilter::ButtonHoverFilter(QString icon, QString hoverIcon, QAbstractButton *button)
	: QObject(button)
	, m_icon(std::move(icon))
	, m_hoverIcon(std::move(hoverIcon))
{
	button->setIcon(QPixmap(m_icon).scaled(button->width(), button->height(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
	button->installEventFilter(this);
}

bool ButtonHoverFilter::eventFilter(QObject *watched, QEvent *event)
{
	QAbstractButton *button = qobject_cast<QAbstractButton*>(watched);
	if(!button)
		return false;
	switch(event->type())
	{
	case QEvent::Enter:
		button->setIcon(QPixmap(m_hoverIcon).scaled(button->width(), button->height(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
		return true;
	case QEvent::Leave:
		button->setIcon(QPixmap(m_icon).scaled(button->width(), button->height(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
		return true;
	default: return false;
	}
}
