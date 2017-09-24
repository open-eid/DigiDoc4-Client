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
#include "Colors.h"
#include "Styles.h"

#include <QPainter>

using namespace ria::qdigidoc4;

PageIcon::PageIcon(QWidget *parent) :
	QWidget(parent),
	ui(new Ui::PageIcon)
{
	ui->setupUi(this);

	icon.reset( new QSvgWidget( this ) );
	icon->resize( 48, 38 );
	icon->move( 31, 23 );
}

PageIcon::~PageIcon()
{
	delete ui;
}

void PageIcon::init( Pages type, QWidget *shadow,  bool selected )
{
	QFont font = Styles::font( Styles::Condensed, 16 );
	switch( type )
	{
	case CryptoIntro:
		active = PageIcon::Style { font, "/images/icon_Krypto_hover.svg", colors::WHITE, colors::CLAY_CREEK };
		hover = PageIcon::Style { font, "/images/icon_Krypto_hover.svg", colors::BAHAMA_BLUE, colors::WHITE };
		inactive = PageIcon::Style { font, "/images/icon_Krypto.svg", colors::ASTRONAUT_BLUE, colors::WHITE };
		icon->resize( 34, 38 );
		icon->move( 38, 26 );	
		ui->label->setText( "KRÜPTO" );
		break;
	case MyEid:
		active = PageIcon::Style { font, "/images/icon_Minu_eID_hover.svg", colors::WHITE, colors::CLAY_CREEK };
		hover = PageIcon::Style { font, "/images/icon_Minu_eID_hover.svg", colors::BAHAMA_BLUE, colors::WHITE };
		inactive = PageIcon::Style { font, "/images/icon_Minu_eID.svg", colors::ASTRONAUT_BLUE, colors::WHITE };
		icon->resize( 44, 31 );
		icon->move( 33, 28 );	
		ui->label->setText( "MINU eID" );
		break;
	default:
		active = PageIcon::Style { font, "/images/icon_Allkiri_hover.svg", colors::WHITE, colors::CLAY_CREEK };
		hover = PageIcon::Style { font, "/images/icon_Allkiri_hover.svg", colors::BAHAMA_BLUE, colors::WHITE };
		inactive = PageIcon::Style { font, "/images/icon_Allkiri.svg", colors::ASTRONAUT_BLUE, colors::WHITE };
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

void PageIcon::enterEvent( QEvent *ev )
{
	if( !selected )
	{
		icon->setStyleSheet(QString("background-color: %1; border: none;").arg(hover.backColor));	
		setStyleSheet(QString("background-repeat: none; background-color: %1; border: none;").arg(hover.backColor));
		ui->label->setStyleSheet( QString("background-color: %1; color: %2; border: none;").arg(hover.backColor).arg(hover.foreColor) );
	}
}

void PageIcon::leaveEvent( QEvent *ev )
{
	if( !selected )
	{
		icon->setStyleSheet(QString("background-color: %1; border: none;").arg(inactive.backColor));	
		setStyleSheet(QString("background-repeat: none; background-color: %1; border: none;").arg(inactive.backColor));
		ui->label->setStyleSheet( QString("background-color: %1; color: %2; border: none;").arg(inactive.backColor).arg(inactive.foreColor) );
	}
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
