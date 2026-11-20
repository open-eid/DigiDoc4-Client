// SPDX-FileCopyrightText: Estonian Information System Authority
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "StyledWidget.h"

#include <QPainter>
#include <QStyleOption>

StyledWidget::StyledWidget(QWidget *parent) :
	QWidget(parent)
{
}

StyledWidget::~StyledWidget() = default;

// Custom widget must override paintEvent in order to use stylesheets
// See https://wiki.qt.io/How_to_Change_the_Background_Color_of_QWidget
void StyledWidget::paintEvent(QPaintEvent */*ev*/)
{
	QStyleOption opt;
	opt.initFrom(this);
	QPainter p(this);
	style()->drawPrimitive(QStyle::PE_Widget, &opt, &p, this);
}

void StyledWidget::stateChange(ria::qdigidoc4::ContainerState /*state*/)
{}
