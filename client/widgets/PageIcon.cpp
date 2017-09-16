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

#include "PageIcon.h"
#include "ui_PageIcon.h"
#include "Styles.h"

#include <QPainter>

using namespace ria::qdigidoc4;

PageIcon::PageIcon(QWidget *parent) :
	QWidget(parent),
	ui(new Ui::PageIcon)
{
	ui->setupUi(this);

	icon = new QSvgWidget( this );
	icon->resize( 48, 38 );
	icon->move( 31, 23 );
}

PageIcon::~PageIcon()
{
	delete ui;
	delete icon;
}

void PageIcon::init( Pages type, QWidget *shadow,  bool selected )
{
	QFont font = Styles::font( Styles::Condensed, 16 );
	switch( type )
	{
	case CryptoIntro:
		active = PageIcon::Style { font, "/images/crypto_dark.svg", "#ffffff", "#998B66" };
		inactive = PageIcon::Style { font, "/images/crypto_light.svg", "#023664", "#ffffff" };
		ui->label->setText( "KRÜPTO" );
		icon->resize( 42, 38 );
		icon->move( 34, 24 );	
		break;
	case MyEid:
		active = PageIcon::Style { font, "/images/my_eid_dark.svg", "#ffffff", "#998B66" };
		inactive = PageIcon::Style { font, "/images/my_eid_light.svg", "#023664", "#ffffff" };
		ui->label->setText( "MINU eID" );
		break;
	default:
		active = PageIcon::Style { font, "/images/sign_dark.svg", "#ffffff", "#998B66" };
		inactive = PageIcon::Style { font, "/images/sign_light.svg", "#023664", "#ffffff" };
		ui->label->setText( "ALLKIRI" );
		break;
	}

	this->selected = selected;
    this->shadow = shadow;
	this->type = type;
	updateSelection();
}

void PageIcon::activate( bool selected )
{
	this->selected = selected;
	updateSelection();
}

Pages PageIcon::getType()
{
	return type;
}

void PageIcon::mouseReleaseEvent(QMouseEvent *event)
{
	emit activated(this);
}

void PageIcon::updateSelection()
{
	const Style &style = selected ? active : inactive;
	if (selected)
	{
		shadow->show();
		shadow->raise();
		raise();
	}
	else
	{
		shadow->hide();
	}

	ui->label->setFont(style.font);
	ui->label->setStyleSheet( QString("background-color: %1; color: %2; border: none;").arg(style.backColor).arg(style.foreColor) );
	icon->load( QString( ":%1" ).arg( style.image ) );
	icon->setStyleSheet(QString("background-color: %1; border: none;").arg(style.backColor));	
	setStyleSheet(QString("background-repeat: none; background-color: %1; border: none;").arg(style.backColor));
}

// Custom widget must override paintEvent in order to use stylesheets
// See https://wiki.qt.io/How_to_Change_the_Background_Color_of_QWidget
void PageIcon::paintEvent(QPaintEvent *ev)
{
	QStyleOption opt;
	opt.init(this);
	QPainter p(this);
	style()->drawPrimitive(QStyle::PE_Widget, &opt, &p, this);
}
