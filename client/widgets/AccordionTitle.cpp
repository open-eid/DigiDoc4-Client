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


#include "AccordionTitle.h"
#include "ui_AccordionTitle.h"

#include "Styles.h"

#include <QtGui/QKeyEvent>

AccordionTitle::AccordionTitle(QWidget *parent)
	: StyledWidget(parent)
	, ui(new Ui::AccordionTitle)
{
	ui->setupUi(this);
	ui->label->setFont( Styles::font( Styles::Condensed, 16 ) );
	ui->label->setStyleSheet(QStringLiteral("border: none; color: #006EB5;"));
	ui->icon->load(QStringLiteral(":/images/accordion_arrow_down.svg"));
}

AccordionTitle::~AccordionTitle()
{
	delete ui;
}

bool AccordionTitle::event(QEvent *e)
{
	switch(e->type())
	{
	case QEvent::MouseButtonRelease:
		setSectionOpen(!isOpen());
		break;
	case QEvent::KeyRelease:
		if(QKeyEvent *ke = static_cast<QKeyEvent*>(e))
			if(ke->key() == Qt::Key_Enter || ke->key() == Qt::Key_Space)
				setSectionOpen(!isOpen());
		break;
	default: break;
	}
	return StyledWidget::event(e);
}

void AccordionTitle::init(bool open, const QString &caption, QWidget *content)
{
	init(open, caption, caption.toLower(), content);
}

void AccordionTitle::init(bool open, const QString &caption, const QString &accessible, QWidget *content)
{
	setText(caption, accessible);
	this->content = content;
	setSectionOpen(open);
}

bool AccordionTitle::isOpen() const
{
	return _isOpen;
}

void AccordionTitle::openSection(AccordionTitle *opened)
{
	setSectionOpen(this == opened);
}

void AccordionTitle::setSectionOpen(bool open)
{
	if(_isOpen == open)
		return;
	_isOpen = open;
	if(content)
		content->setVisible(open && !isHidden());
	if(open)
	{
		ui->label->setStyleSheet(QStringLiteral("border: none; color: #006EB5;"));
		ui->icon->resize( 12, 6 );
		ui->icon->move(15, 17);
		ui->icon->load(QStringLiteral(":/images/accordion_arrow_down.svg"));
		emit opened(this);
	}
	else
	{
		ui->label->setStyleSheet(QStringLiteral("border: none; color: #353739;"));
		ui->icon->resize( 6, 12 );
		ui->icon->move(18, 14);
		ui->icon->load(QStringLiteral(":/images/accordion_arrow_right.svg"));
		emit closed(this);
	}
}

void AccordionTitle::setText(const QString &caption, const QString &accessible)
{
	ui->label->setText(caption);
	ui->label->setAccessibleName(accessible);
}

void AccordionTitle::setVisible(bool visible)
{
	StyledWidget::setVisible(visible);
	if(content)
		content->setVisible(visible && isOpen());
}
