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

#include <QEasingCurve>
#include <QGraphicsOpacityEffect>
#include <QPropertyAnimation>
#include <QResizeEvent>
#include <QTimer>

using namespace std::chrono;

constexpr QRect adjustHeight(QRect rect, int height) noexcept
{
	rect.setHeight(height);
	return rect;
}

FadeInNotification::FadeInNotification(QWidget *parent, QRect rect, Type type, const QString &label)
	: QLabel(parent)
{
	auto bgcolor = [type] {
		switch(type) {
		case FadeInNotification::Success: return QStringLiteral("#218123");
		case FadeInNotification::Warning: return QStringLiteral("#FBAE38");
		case FadeInNotification::Error: return QStringLiteral("#AD2A45");
		case FadeInNotification::Default: return QStringLiteral("#2F70B6");
		default: return QStringLiteral("none");
		}
	}();
	auto fgcolor = [type] {
		return type == FadeInNotification::Warning ? QStringLiteral("#07142A") : QStringLiteral("#FFFFFF");
	}();
	setStyleSheet(QStringLiteral("color: %1; background-color: %2;").arg(fgcolor, bgcolor));
	setFocusPolicy(Qt::TabFocus);
	setAlignment(Qt::AlignHCenter | Qt::AlignVCenter);
	QFont f(QStringLiteral("Roboto"));
	f.setPixelSize(22);
	setFont(f);
	setGraphicsEffect(new QGraphicsOpacityEffect(this));
	setGeometry(rect);
	setText(label);
	parent->installEventFilter(this);
}

bool FadeInNotification::eventFilter(QObject *watched, QEvent *event)
{
	if(watched == parent() && event->type() == QEvent::Resize)
	{
		if(auto *resize = static_cast<QResizeEvent*>(event))
		{
			QRect rect = geometry();
			if(QSize s = resize->oldSize() / 2; rect.x() > s.width() && rect.y() > s.height())
			{
				QSize newPos = resize->size() - resize->oldSize();
				move(pos() + QPoint(newPos.width(), newPos.height()));
			}
			else
			{
				rect.setWidth(parentWidget()->width());
				setGeometry(rect);
			}
		}
	}
	return QLabel::eventFilter(watched, event);
}

void FadeInNotification::start(ms fadeInTime)
{
	auto *a = new QPropertyAnimation(graphicsEffect(), "opacity", this);
	a->setDuration(int(fadeInTime.count()));
	a->setStartValue(0);
	a->setEndValue(1);
	a->setEasingCurve(QEasingCurve::InBack);
	a->start(QPropertyAnimation::DeleteWhenStopped);
	connect(a, &QPropertyAnimation::finished, this, [this] {
		if(focusPolicy() == Qt::TabFocus)
			setFocus();
		QTimer::singleShot(3s, this, [this] {
			auto *a = new QPropertyAnimation(graphicsEffect(), "opacity", this);
			a->setDuration(int((1200ms).count()));
			a->setStartValue(1);
			a->setEndValue(0);
			a->setEasingCurve(QEasingCurve::OutBack);
			a->start(QPropertyAnimation::DeleteWhenStopped);
			connect(a, &QPropertyAnimation::finished, this, &FadeInNotification::deleteLater);
		});
	});
	show();
}

void FadeInNotification::success(QWidget *parent, const QString &label)
{
	(new FadeInNotification(parent, adjustHeight(parent->rect(), 65), FadeInNotification::Success, label))->start();
}

void FadeInNotification::warning(QWidget *parent, const QString &label)
{
	(new FadeInNotification(parent, adjustHeight(parent->rect(), 65), FadeInNotification::Warning, label))->start();
}

void FadeInNotification::error(QWidget *parent, const QString &label, int height)
{
	(new FadeInNotification(parent, adjustHeight(parent->rect(), height), FadeInNotification::Error, label))->start();
}
