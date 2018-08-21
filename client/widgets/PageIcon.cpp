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

PageIcon::PageIcon(QWidget *parent)
: StyledWidget(parent)
, ui(new Ui::PageIcon)
, iconType(None)
, selected(false)
, type(Pages::SignIntro)
{
	ui->setupUi(this);

	icon.reset( new QSvgWidget( this ) );
	icon->resize( 48, 38 );
	icon->move( 31, 23 );

	brightRedIcon.reset(new QSvgWidget(QStringLiteral(":/images/icon_alert_filled_red.svg"), this));
	brightRedIcon->resize( 13, 12 );
	brightRedIcon->move( 84, 12 );
	brightRedIcon->hide();
	brightRedIcon->setStyleSheet(QStringLiteral("border: none;"));

	redIcon.reset(new QSvgWidget(QStringLiteral(":/images/icon_alert_red.svg"), this));
	redIcon->resize( 13, 12 );
	redIcon->move( 84, 12 );
	redIcon->hide();
	redIcon->setStyleSheet(QStringLiteral("border: none;"));

	filledOrangeIcon.reset(new QSvgWidget(QStringLiteral(":/images/icon_alert_filled_orange.svg"), this));
	filledOrangeIcon->resize( 13, 12 );
	filledOrangeIcon->move( 84, 12 );
	filledOrangeIcon->hide();
	filledOrangeIcon->setStyleSheet(QStringLiteral("border: none;"));

	orangeIcon.reset(new QSvgWidget(QStringLiteral(":/images/icon_alert_whitebg_orange.svg"), this));
	orangeIcon->resize( 13, 12 );
	orangeIcon->move( 84, 12 );
	orangeIcon->hide();
	orangeIcon->setStyleSheet(QStringLiteral("border: none;"));
}

PageIcon::~PageIcon()
{
	delete ui;
}

void PageIcon::init( Pages type, QWidget *shadow, bool selected )
{
	static const QString BOTTOM_BORDER = QStringLiteral("solid rgba(255, 255, 255, 0.1); border-width: 0px 0px 1px 0px");
	static const QString RIGHT_BORDER = QStringLiteral("solid #E7E7E7; border-width: 0px 1px 0px 0px");

	QFont font = Styles::font( Styles::Condensed, 16 );
	switch( type )
	{
	case CryptoIntro:
		active = PageIcon::Style { font, "/images/icon_Krypto_hover.svg", colors::WHITE, colors::CLAY_CREEK, RIGHT_BORDER };
		hover = PageIcon::Style { font, "/images/icon_Krypto_hover.svg", colors::BAHAMA_BLUE, colors::WHITE, "none" };
		inactive = PageIcon::Style { font, "/images/icon_Krypto.svg", colors::ASTRONAUT_BLUE, colors::WHITE, BOTTOM_BORDER };
		icon->resize( 34, 38 );
		icon->move( 38, 26 );	
		ui->label->setText( tr("CRYPTO") );
		break;
	case MyEid:
		active = PageIcon::Style { font, "/images/icon_Minu_eID_hover.svg", colors::WHITE, colors::CLAY_CREEK, RIGHT_BORDER };
		hover = PageIcon::Style { font, "/images/icon_Minu_eID_hover.svg", colors::BAHAMA_BLUE, colors::WHITE, "none" };
		inactive = PageIcon::Style { font, "/images/icon_Minu_eID.svg", colors::ASTRONAUT_BLUE, colors::WHITE, BOTTOM_BORDER };
		icon->resize( 44, 31 );
		icon->move( 33, 28 );	
		ui->label->setText( tr("My eID") );
		break;
	default:
		active = PageIcon::Style { font, "/images/icon_Allkiri_hover.svg", colors::WHITE, colors::CLAY_CREEK, RIGHT_BORDER };
		hover = PageIcon::Style { font, "/images/icon_Allkiri_hover.svg", colors::BAHAMA_BLUE, colors::WHITE, "none" };
		inactive = PageIcon::Style { font, "/images/icon_Allkiri.svg", colors::ASTRONAUT_BLUE, colors::WHITE, BOTTOM_BORDER };
		ui->label->setText( tr("SIGNATURE") );
		break;
	}

	this->selected = selected;
	this->shadow = shadow;
	this->type = type;
	updateSelection();
}

void PageIcon::changeEvent(QEvent* event)
{
	if (event->type() == QEvent::LanguageChange)
	{
		ui->retranslateUi(this);
		switch( type )
		{
		case CryptoIntro:
			ui->label->setText( tr("CRYPTO") );
			break;
		case MyEid:
			ui->label->setText( tr("My eID") );
			break;
		default:
			ui->label->setText( tr("SIGNATURE") );
			break;
		}
	}

	QWidget::changeEvent(event);
}

void PageIcon::activate( bool selected )
{
	this->selected = selected;
	updateIcon();
	updateSelection();
}

void PageIcon::enterEvent(QEvent *)
{
	if( !selected )
	{
		updateSelection(hover);
	}
}

Pages PageIcon::getType()
{
	return type;
}

void PageIcon::invalidIcon(bool show)
{
	iconType = show ? Error : None;
	updateIcon();
}

void PageIcon::leaveEvent(QEvent *)
{
	if( !selected )
	{
		updateSelection(inactive);
	}
}

void PageIcon::mouseReleaseEvent(QMouseEvent *)
{
	emit activated(this);
}

void PageIcon::updateIcon()
{
	redIcon->setVisible(iconType == Error && selected);
	brightRedIcon->setVisible(iconType == Error && !selected);
	orangeIcon->setVisible(iconType == Warning && selected);
	filledOrangeIcon->setVisible(iconType == Warning && !selected);
}

void PageIcon::updateSelection()
{
	const Style &style = selected ? active : inactive;
	shadow->setVisible(selected);
	ui->label->setFont(style.font);
	icon->load(QStringLiteral(":%1").arg(style.image));
	updateSelection(style);
}

void PageIcon::updateSelection(const Style &style)
{
	ui->label->setStyleSheet(QStringLiteral("background-color: %1; color: %2; border: none;").arg(style.backColor, style.foreColor));
	icon->setStyleSheet(QStringLiteral("background-color: %1; border: none;").arg(style.backColor));
	setStyleSheet(QStringLiteral("background-repeat: none; background-color: %1; border: %2;").arg(style.backColor, style.border));
}

void PageIcon::warningIcon(bool show)
{
	if(iconType != Error)
		iconType = show ? Warning : None;
	updateIcon();
}
