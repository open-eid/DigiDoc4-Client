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
	ui->icon->resize( 12, 6 );
	ui->icon->move( 15, 17 );
	ui->label->setFont( Styles::font( Styles::Condensed, 16 ) );
}

AccordionTitle::~AccordionTitle()
{
	delete ui;
}

void AccordionTitle::borderless()
{
	setStyleSheet(QStringLiteral("background-color: #FFFFFF; border: none;"));
}

bool AccordionTitle::event(QEvent *e)
{
	auto process = [&]{
		if(!content->isVisible())
		{
			setSectionOpen(true);
			emit opened(this);
		}
		else if(closable)
		{
			setSectionOpen(false);
			emit closed(this);
		}
		else
			return StyledWidget::event(e);
		return true;
	};
	switch(e->type())
	{
	case QEvent::MouseButtonRelease:
		return process();
	case QEvent::KeyRelease:
		if(QKeyEvent *ke = static_cast<QKeyEvent*>(e))
			if(ke->key() == Qt::Key_Enter || ke->key() == Qt::Key_Space)
				return process();
		break;
	default: break;
	}
	return StyledWidget::event(e);
}

void AccordionTitle::init(bool open, const QString &caption, const QString &accessible, QWidget *content)
{
	setText(caption, accessible);
	this->content = content;
	setSectionOpen(open);
}

void AccordionTitle::setSectionOpen(bool open)
{
	content->setVisible(open);
	if(open)
	{
		ui->label->setStyleSheet(QStringLiteral("border: none; color: #006EB5;"));
		ui->icon->resize( 12, 6 );
		ui->icon->move( 15, 17 );
		ui->icon->load(QStringLiteral(":/images/accordion_arrow_down.svg"));
	}
	else
	{
		ui->label->setStyleSheet(QStringLiteral("border: none; color: #353739;"));
		ui->icon->resize( 6, 12 );
		ui->icon->move( 18, 14 );
		ui->icon->load(QStringLiteral(":/images/accordion_arrow_right.svg"));
	}
}

void AccordionTitle::setClosable(bool closable)
{
	this->closable = closable;
}

void AccordionTitle::setText(const QString &caption, const QString &accessible)
{
	ui->label->setText(caption);
	ui->label->setAccessibleName(accessible);
}
