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

#include "LabelButton.h"
#include "Colors.h"
#include "Styles.h"

#include <QEvent>


using namespace ria::qdigidoc4::colors;

LabelButton::LabelButton( QWidget *parent ): QLabel(parent)
{}

void LabelButton::init( Style style, const QString &label, int code )
{
	static const QString borderRadius = QStringLiteral(" border-radius: 2px;");
	static const QString styleTemplate = QStringLiteral("QLabel { background-color: %1; color: %2;%4 border: %3; text-decoration: none solid; }");
	this->code = code;
	this->style = style;
	QString bgColor = PORCELAIN;

	switch( style )
	{
		case BoxedDeepCerulean:
			css[Normal].style  = styleTemplate.arg(bgColor, DEEP_CERULEAN, QStringLiteral("none"), borderRadius);
			css[Hover].style   = styleTemplate.arg(bgColor, DEEP_CERULEAN, QStringLiteral("1px solid %1").arg(DEEP_CERULEAN), borderRadius);
			css[Pressed].style = styleTemplate.arg(DEEP_CERULEAN, bgColor, QStringLiteral("none"), borderRadius);
			break;
		case BoxedMojo:
			css[Normal].style  = styleTemplate.arg(bgColor, MOJO, QStringLiteral("none"), borderRadius);
			css[Hover].style   = styleTemplate.arg(bgColor, MOJO, QStringLiteral("1px solid %1").arg(MOJO), borderRadius);
			css[Pressed].style = styleTemplate.arg(MOJO, bgColor, QStringLiteral("none"), borderRadius);
			break;
		case BoxedDeepCeruleanWithCuriousBlue:
			// Edit
			css[Normal].style  = styleTemplate.arg(bgColor, DEEP_CERULEAN, QStringLiteral("1px solid %1").arg(bgColor), borderRadius);
			css[Hover].style   = styleTemplate.arg(bgColor, CURIOUS_BLUE, QStringLiteral("1px solid %1").arg(CURIOUS_BLUE), borderRadius);
			css[Pressed].style = styleTemplate.arg(CURIOUS_BLUE, bgColor, QStringLiteral("1px solid %1").arg(CURIOUS_BLUE), borderRadius);
			css[Pressed].background = CURIOUS_BLUE;
			break;
		case DeepCeruleanWithLochmara:
			// Add files
			bgColor = WHITE;
			css[Normal].style  = styleTemplate.arg(bgColor, DEEP_CERULEAN, QStringLiteral("none"), QString());
			css[Hover].style   = styleTemplate.arg(DEEP_CERULEAN, bgColor, QStringLiteral("none"), QString());
			css[Pressed].style = styleTemplate.arg(CURIOUS_BLUE, bgColor, QStringLiteral("none"), QString());
			break;
		case White:
			// Add files
			bgColor = WHITE;
			css[Normal].style  = QStringLiteral("border: none; background-color: none;");
			css[Hover].style   = QStringLiteral("border: none; background-color: none;");
			css[Pressed].style = QStringLiteral("border: none; background-color: none;");
			break;
		case None:
			// Add files
			css[Normal].style  = QString();
			css[Hover].style   = QString();
			css[Pressed].style = QString();
			break;
	}

	css[ Normal ].background = bgColor;
	css[ Hover ].background = bgColor;
	setText( label );
	if(!label.isEmpty())
		setAccessibleName(label.toLower());
	setFont( Styles::font( Styles::Condensed, 12 ) );
	normal();
}

void LabelButton::enterEvent(QEvent * /*ev*/)
{
	hover();
	emit entered();
}

void LabelButton::leaveEvent(QEvent * /*ev*/)
{
	normal();
	emit left();
}

void LabelButton::normal()
{
	if ( css[Normal].style.isEmpty() )
		return;

	setStyleSheet( css[Normal].style );

	if( icon )
	{
		icon->load(QStringLiteral(":%1").arg(css[Normal].icon));
		icon->setStyleSheet(QStringLiteral("background-color: %1; border: none;").arg(css[Normal].background));
	}
}

void LabelButton::hover()
{
	if ( css[Hover].style.isEmpty() )
		return;

	setStyleSheet( css[Hover].style );

	if( icon )
	{
		icon->load(QStringLiteral(":%1").arg(css[Hover].icon));
		icon->setStyleSheet(QStringLiteral("background-color: %1; border: none;").arg(css[Hover].background));
	}
}

void LabelButton::pressed()
{
	if ( css[Pressed].style.isEmpty() )
		return;

	setStyleSheet( css[Pressed].style );

	if( icon )
	{
		icon->load(QStringLiteral(":%1").arg(css[Pressed].icon));
		icon->setStyleSheet(QStringLiteral("background-color: %1; border: none;").arg(css[Pressed].background));
	}
}

void LabelButton::setIcons( const QString &normalIcon, const QString &hoverIcon, const QString &pressedIcon, int x, int y, int w, int h )
{
	css[ Normal ].icon = normalIcon;
	css[ Hover ].icon = hoverIcon;
	css[ Pressed ].icon = pressedIcon;

	icon.reset(new QSvgWidget(QStringLiteral(":%1").arg(css[Normal].icon), this));
	icon->resize( w, h );
	icon->move( x, y );
}

bool LabelButton::event(QEvent *ev)
{
	switch(ev->type())
	{
	case QEvent::MouseButtonPress:
		pressed();
		return true;
	case QEvent::MouseButtonRelease:
		hover();
		emit clicked(code);
		return true;
	default: return QLabel::event(ev);
	}
}
