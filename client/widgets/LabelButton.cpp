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

const QString LabelButton::styleTemplate( "QLabel { background-color: %1; color: %2; border-radius: 2px; border: %3; text-decoration: none solid; }" );

LabelButton::LabelButton( QWidget *parent )
: QLabel( parent )
{
}

void LabelButton::init( Style style, const QString &label, int code )
{
    this->code = code;
    this->style = style;
    QString bgColor = PORCELAIN;
    
    switch( style )
    {
        case BoxedDeepCerulean:
            css[ Normal ].style  = styleTemplate.arg( bgColor, DEEP_CERULEAN, "none" );
            css[ Hover ].style   = styleTemplate.arg( bgColor, DEEP_CERULEAN, QString("1px solid %1").arg( DEEP_CERULEAN ) );
            css[ Pressed ].style = styleTemplate.arg( DEEP_CERULEAN, bgColor, "none" );
            break;
        case BoxedMojo:
            css[ Normal ].style  = styleTemplate.arg( bgColor, MOJO, "none" );
            css[ Hover ].style   = styleTemplate.arg( bgColor, MOJO, QString("1px solid %1").arg( MOJO ) );
            css[ Pressed ].style = styleTemplate.arg( MOJO, bgColor, "none" );
            break;
        case BoxedDeepCeruleanWithCuriousBlue:
            // Edit
            css[ Normal ].style  = styleTemplate.arg( bgColor, DEEP_CERULEAN, "none" );
            css[ Hover ].style   = styleTemplate.arg( bgColor, CURIOUS_BLUE, QString("1px solid %1").arg( CURIOUS_BLUE ) );
            css[ Pressed ].style = styleTemplate.arg( CURIOUS_BLUE, bgColor, "none" );
            css[ Pressed ].background = CURIOUS_BLUE;
            break;
        case DeepCeruleanWithLochmara:
            // Add files
            bgColor = WHITE;
            css[ Normal ].style  = styleTemplate.arg( bgColor, DEEP_CERULEAN, "none" );
            css[ Hover ].style   = styleTemplate.arg( DEEP_CERULEAN, bgColor, "none" );
            css[ Pressed ].style = styleTemplate.arg( CURIOUS_BLUE, bgColor, "none" );
            break;
        case None:
            // Add files
            css[ Normal ].style  = QString();
            css[ Hover ].style   = QString();
            css[ Pressed ].style = QString();
            break;
    }

    css[ Normal ].background = bgColor;
    css[ Hover ].background = bgColor;
    setText( label );
	setFont( Styles::font( Styles::Condensed, 12 ) );
    normal();
}

void LabelButton::enterEvent( QEvent *ev )
{
    hover();
}

void LabelButton::leaveEvent( QEvent *ev )
{
    normal();
}

void LabelButton::normal()
{
    if ( css[Normal].style.isEmpty() )
        return;

    setStyleSheet( css[Normal].style );

    if( icon )
    {
        icon->load( QString( ":%1" ).arg( css[Normal].icon ) );
        icon->setStyleSheet(QString("background-color: %1; border: none;").arg( css[Normal].background ));
    }
}

void LabelButton::hover()
{
    if ( css[Hover].style.isEmpty() )
        return;

    setStyleSheet( css[Hover].style );

    if( icon )
    {
        icon->load( QString( ":%1" ).arg( css[Hover].icon ) );
        icon->setStyleSheet(QString("background-color: %1; border: none;").arg( css[Hover].background ));
    }
}

void LabelButton::pressed()
{
    if ( css[Pressed].style.isEmpty() )
        return;

    setStyleSheet( css[Pressed].style );

    if( icon )
    {
        icon->load( QString( ":%1" ).arg( css[Pressed].icon ) );
        icon->setStyleSheet(QString("background-color: %1; border: none;").arg( css[Pressed].background ));
    }
}

void LabelButton::setIcons( const QString &normalIcon, const QString &hoverIcon, const QString &pressedIcon, int x, int y, int w, int h )
{
    css[ Normal ].icon = normalIcon;
    css[ Hover ].icon = hoverIcon;
    css[ Pressed ].icon = pressedIcon;

    icon.reset( new QSvgWidget( QString( ":%1" ).arg( css[Normal].icon ), this ) );
    icon->resize( w, h );
    icon->move( x, y );
}

bool LabelButton::event( QEvent *ev )  
{
    // Identify Mouse press Event
    if( ev->type() == QEvent::MouseButtonPress )
    {
        pressed();
    }
    else if( ev->type() == QEvent::MouseButtonRelease )
    {
        hover();
        emit clicked(code);
    }
    return QLabel::event( ev );
}
