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
#include "widgets/Accordion.h"


AccordionTitle::AccordionTitle(QWidget *parent) :
	StyledWidget(parent),
	ui(new Ui::AccordionTitle),
	closable(false)
{
	ui->setupUi(this);
	icon.reset( new QSvgWidget( this ) );
	icon->setStyleSheet( "border: none;" );
	icon->resize( 12, 6 );
	icon->move( 15, 17 );
	ui->label->setFont( Styles::font( Styles::Condensed, 16 ) );
}

AccordionTitle::~AccordionTitle()
{
	delete ui;
}

void AccordionTitle::borderless()
{
	setStyleSheet("background-color: #FFFFFF; border: none;");
}

void AccordionTitle::closeSection()
{
	content->setVisible(false);
	ui->label->setStyleSheet("border: none; color: #353739;");
	icon->resize( 6, 12 );
	icon->move( 18, 14 );
	icon->load( QString( ":/images/accordion_arrow_right.svg" ) );	
}

void AccordionTitle::init(bool open, const QString& caption, QWidget* content)
{
	ui->label->setText(caption);
	this->content = content;
	if(open)
		openSection();
	else
		closeSection();
}

void AccordionTitle::mouseReleaseEvent(QMouseEvent *event)
{
	if(!content->isVisible())
	{
		content->setVisible(true);
		emit opened(this);
		openSection();
	}
	else if(closable)
	{
		content->setVisible(false);
		closeSection();
		emit closed(this);
	}
}

void AccordionTitle::openSection()
{
	content->setVisible(true);
	ui->label->setStyleSheet("border: none; color: #006EB5;");
	icon->resize( 12, 6 );
	icon->move( 15, 17 );
	icon->load( QString( ":/images/accordion_arrow_down.svg" ) );
}

void AccordionTitle::setClosable(bool closable)
{
	this->closable = closable;
}

void AccordionTitle::setText(const QString& caption)
{
	ui->label->setText(caption);
}
