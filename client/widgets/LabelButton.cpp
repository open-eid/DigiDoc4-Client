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
#include "Styles.h"

#include <QEvent>

const QString LabelButton::styleTemplate( "QLabel { background-color: %1; color: %2; border-radius: 3px; border: none; text-decoration: none solid; }" );

LabelButton::LabelButton( QWidget *parent )
: QLabel( parent )
{
}

void LabelButton::init( int code )
{
    this->code = code;
}

void LabelButton::init( int style, const QString &label, int code )
{
    QString bgColor = background( style );
    QString fgColor = foreground( style );

    this->code = code;
    this->style = style;
    normalStyle = styleTemplate.arg( bgColor, fgColor );
    hoverStyle = styleTemplate.arg( fgColor, bgColor );
    normal();
    setText( label );
	setFont( Styles::font( Styles::Condensed, 12 ) );
}

void LabelButton::setStyles( const QString &nStyle, const QString &nLink, const QString &hStyle, const QString &hLink )
{
    normalStyle = nStyle;
    hoverStyle = hStyle;
}

QString LabelButton::background(int style) const
{
    if ( style & WhiteBackground ) {
        return "#ffffff";
    } else if( style & AlabasterBackground ) {
        return "#fafafa";
    } else if( style & PorcelainBackground ) {
        return "#f4f5f6";
    } else {
        return "#f7f7f7";
    }
}

QString LabelButton::foreground(int style) const
{
    if (style & Mojo) {
        return "#c53e3e";
    } else {
        return "#006eb5";
    }
}

void LabelButton::enterEvent( QEvent *ev )
{
    setStyleSheet( hoverStyle );

    if( icon )
    {
        icon->load( QString( ":%1" ).arg( hoverIcon ) );
        icon->setStyleSheet(QString("background-color: %1; border: none;").arg( foreground( style ) ));
    }
}

void LabelButton::leaveEvent( QEvent *ev )
{
    normal();
}

void LabelButton::normal()
{
    setStyleSheet( normalStyle );

    if( icon )
    {
        icon->load( QString( ":%1" ).arg( normalIcon ) );
        icon->setStyleSheet(QString("background-color: %1; border: none;").arg( background( style ) ));
    }
}

void LabelButton::setIcons( const QString &normalIcon, const QString &hoverIcon, int x, int y, int w, int h )
{
    icon.reset( new QSvgWidget( QString( ":%1" ).arg( normalIcon ), this ) );
    icon->resize( w, h );
    icon->move( x, y );
    this->normalIcon = normalIcon;
    this->hoverIcon = hoverIcon;
}

bool LabelButton::event( QEvent *ev )  
{
    // Identify Mouse press Event
    if( ev->type() == QEvent::MouseButtonRelease )
    {
        emit clicked(code);
    }
    return QLabel::event( ev );
}
