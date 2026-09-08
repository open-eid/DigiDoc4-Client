// SPDX-FileCopyrightText: Estonian Information System Authority
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "MainAction.h"

#include <QEvent>

using namespace ria::qdigidoc4;

MainAction::MainAction(QWidget *parent)
	: QPushButton(parent)
{
    setFixedSize(QSize(200, 65));
    setCursor(QCursor(Qt::CursorShape::PointingHandCursor));
    setStyleSheet(QString::fromUtf8(R"(QPushButton {
border: 0px;
color: #ffffff;
background-color: #2F70B6;
font-family: Roboto, Helvetica;
font-size: 16px;
font-weight: 700;
border-top-left-radius: 4px;
}
QPushButton:hover, QPushButton:focus {
background-color: #2B66A6;
}
QPushButton:pressed {
background-color: #215081;
}
QPushButton:disabled {
background-color: #82A9D3;
})"));
	parent->installEventFilter(this);
	move(parent->width() - width(), parent->height() - height());
}

void MainAction::changeEvent(QEvent* event)
{
	if(event->type() == QEvent::LanguageChange)
		update();
	QWidget::changeEvent(event);
}

bool MainAction::eventFilter(QObject *watched, QEvent *event)
{
	if(event->type() == QEvent::Resize && watched == parentWidget())
		move(parentWidget()->width() - width(), parentWidget()->height() - height());
	return QWidget::eventFilter(watched, event);
}

void MainAction::setEncrypt(bool encrypt)
{
	isEncrypt = encrypt;
	update();
	show();
}

void MainAction::update()
{
	setText(isEncrypt ? tr("Encrypt") : tr("Sign"));
}
