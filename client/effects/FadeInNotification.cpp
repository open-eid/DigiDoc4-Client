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

#include "FadeInNotification.h"
#include "Styles.h"

#include <QEasingCurve>
#include <QGraphicsOpacityEffect>
#include <QPropertyAnimation>
#include <QResizeEvent>
#include <QTimer>

FadeInNotification::FadeInNotification(QWidget *parent, const QString &fgColor, const QString &bgColor, int leftOffset, int height)
	: FadeInNotification(parent, fgColor, bgColor, QPoint(leftOffset, 0), parent->width() - leftOffset, height)
{
}

FadeInNotification::FadeInNotification(QWidget *parent, const QString &fgColor, const QString &bgColor, QPoint pos, int width, int height)
	: QLabel(parent)
{
	setStyleSheet(QStringLiteral("background-color: %2; color: %1;").arg(fgColor, bgColor));
	setFocusPolicy(Qt::TabFocus);
	setAlignment(Qt::AlignHCenter | Qt::AlignVCenter);
	QFont font = Styles::font(Styles::Condensed, 22);
	font.setPixelSize(22);
	setFont(font);
	setMinimumSize(width, height);
	setGraphicsEffect(new QGraphicsOpacityEffect(this));
	move(pos);
	if(QPoint c = parent->geometry().center(); pos.x() > c.x() && pos.y() > c.y())
		parent->installEventFilter(this);
}

bool FadeInNotification::eventFilter(QObject *watched, QEvent *event)
{
	if(watched == parent() && event->type() == QEvent::Resize)
	{
		if(auto *resize = static_cast<QResizeEvent*>(event))
		{
			QSize newPos = resize->size() - resize->oldSize();
			move(pos() + QPoint(newPos.width(), newPos.height()));
		}
	}
	return QLabel::eventFilter(watched, event);
}

void FadeInNotification::start( const QString &label, int fadeInTime, int displayTime, int fadeOutTime )
{
	setText(label);
	auto *a = new QPropertyAnimation(graphicsEffect(), "opacity", this);
	a->setDuration(fadeInTime);
	a->setStartValue(0);
	a->setEndValue(0.95);
	a->setEasingCurve(QEasingCurve::InBack);
	a->start(QPropertyAnimation::DeleteWhenStopped);
	connect(a, &QPropertyAnimation::finished, this, [this, displayTime, fadeOutTime] {
		if(focusPolicy() == Qt::TabFocus)
			setFocus();
		QTimer::singleShot(displayTime, this, [this, fadeOutTime] {
			auto *a = new QPropertyAnimation(graphicsEffect(), "opacity", this);
			a->setDuration(fadeOutTime);
			a->setStartValue(0.95);
			a->setEndValue(0);
			a->setEasingCurve(QEasingCurve::OutBack);
			a->start(QPropertyAnimation::DeleteWhenStopped);
			connect(a, &QPropertyAnimation::finished, this, &FadeInNotification::deleteLater);
		});
	});
	show();
}
