// SPDX-FileCopyrightText: Estonian Information System Authority
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "Overlay.h"

#include <QGraphicsBlurEffect>
#include <QPainter>

Overlay::Overlay(QWidget *runner)
	: Overlay(runner, runner->parentWidget())
{}

Overlay::Overlay(QWidget *runner, QWidget *parent)
	: QWidget(parent->topLevelWidget())
{
	connect(runner, &QWidget::destroyed, this, &Overlay::deleteLater);
	setPalette(Qt::transparent);
	setAttribute(Qt::WA_TransparentForMouseEvents);
	if(parentWidget())
	{
		setMinimumSize(parentWidget()->size());
		parentWidget()->setGraphicsEffect(new QGraphicsBlurEffect);
	}
	show();
}

Overlay::~Overlay()
{
	if(parentWidget())
		parentWidget()->setGraphicsEffect(nullptr);
}

void Overlay::paintEvent(QPaintEvent * /*event*/)
{
	if(parentWidget())
		setMinimumSize(parentWidget()->size());
	QPainter painter( this );
	painter.setRenderHint( QPainter::Antialiasing );
	// Opacity 90%
	painter.setBrush(QBrush(QColor(0x04, 0x1E, 0x42, int(double(0xff) * 0.9))));
	painter.setPen( Qt::NoPen );
	painter.drawRect( rect() );
}
