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
	connect(this, &QPushButton::clicked, this, [this]{ emit action(_action); });
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

void MainAction::showAction(Actions action)
{
	_action = action;
	update();
	show();
}

void MainAction::update()
{
	switch(_action)
	{
	case EncryptContainer: return setText(tr("Encrypt"));
	default: return setText(tr("Sign"));
	}
}
