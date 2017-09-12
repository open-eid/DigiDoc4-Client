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
#include <QTimer>

FadeInNotification::FadeInNotification(QWidget *parent, const QString &fgColor, const QString &bgColor)
: QLabel(parent)
, fgColor(fgColor)
, bgColor(bgColor)
{
}

void FadeInNotification::start( const QString &label, int fadeInTime, int displayTime, int fadeOutTime )
{
    this->fadeOutTime = fadeOutTime;
    setText(label);
    setAttribute(Qt::WA_DeleteOnClose);
    setStyleSheet(QString("background-color: %2; color: %1;").arg(fgColor, bgColor));
    setAlignment(Qt::AlignHCenter | Qt::AlignVCenter);
    setFont(Styles::instance().font(Styles::OpenSansRegular, 16));
    setMinimumSize(parentWidget()->width(), 50);

    QGraphicsOpacityEffect *effect = new QGraphicsOpacityEffect(this);
    setGraphicsEffect(effect);
    QPropertyAnimation *a = new QPropertyAnimation(effect,"opacity");
    a->setDuration(fadeInTime);
    a->setStartValue(0);
    a->setEndValue(0.95);
    a->setEasingCurve(QEasingCurve::InBack);
    a->start(QPropertyAnimation::DeleteWhenStopped);

    connect( a, &QPropertyAnimation::finished, this, [this, displayTime]() { QTimer::singleShot(displayTime, this, &FadeInNotification::fadeOut); });

    show();
}

void FadeInNotification::fadeOut()
{
    QGraphicsOpacityEffect *effect = new QGraphicsOpacityEffect();
    setGraphicsEffect(effect);
    QPropertyAnimation *a = new QPropertyAnimation(effect,"opacity");
    a->setDuration(fadeOutTime);
    a->setStartValue(0.95);
    a->setEndValue(0);
    a->setEasingCurve(QEasingCurve::OutBack);
    a->start(QPropertyAnimation::DeleteWhenStopped);
    connect( a, &QPropertyAnimation::finished, this, &FadeInNotification::close );
}
