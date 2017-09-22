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

#include "NoCardInfo.h"
#include "ui_NoCardInfo.h"
#include "Styles.h"

NoCardInfo::NoCardInfo( QWidget *parent )
: QWidget( parent )
, ui( new Ui::NoCardInfo )
, cardIcon( new QSvgWidget( this ) )
{
    ui->setupUi( this );
    
    ui->cardStatus->setFont( Styles::font( Styles::Condensed, 16 ) );
    cardIcon->resize( 34, 24 );
    cardIcon->move( 4, 18 );
    cardIcon->load( QString(":/images/icon_IDkaart_red.svg") );
}

NoCardInfo::~NoCardInfo()
{
    delete ui;
}

void NoCardInfo::update( const QString &status )
{
    ui->cardStatus->setText( status );
}
