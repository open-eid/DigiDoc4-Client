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

#include "CardInfo.h"
#include "ui_CardInfo.h"
#include "common_enums.h"
#include "Styles.h"
#include "widgets/LabelButton.h"

using namespace ria::qdigidoc4;

CardInfo::CardInfo( QWidget *parent )
: QWidget( parent )
, ui( new Ui::CardInfo )
{
	ui->setupUi( this );
	QFont font = Styles::font( Styles::Condensed, 16 );

	ui->cardName->setFont( Styles::font( Styles::Condensed, 20, QFont::DemiBold ) );
	ui->cardCode->setFont( font );
	ui->cardStatus->setFont( font );
	ui->cardPhoto->init( LabelButton::None, "", CardPhoto );

	cardIcon.reset( new QSvgWidget( ":/images/icon_IDkaart.svg", this ) );
	cardIcon->resize( 17, 12 );
	cardIcon->move( 159, 42 );

	connect( ui->cardPhoto, &LabelButton::clicked, this, &CardInfo::thePhotoLabelHasBeenClicked );
}

CardInfo::~CardInfo()
{
	delete ui;
}

void CardInfo::clearPicture()
{
	ui->cardPhoto->clear();
}

void CardInfo::update( const QString &name, const QString &code, const QString &status )
{
	ui->cardName->setText( name );
	ui->cardCode->setText( code + "   |" );
	ui->cardStatus->setText( status );
}

void CardInfo::showPicture( const QPixmap &pix )
{
	ui->cardPhoto->setProperty( "PICTURE", pix );
	ui->cardPhoto->setPixmap( pix.scaled( 34, 44, Qt::IgnoreAspectRatio, Qt::SmoothTransformation ) );
}

void CardInfo::thePhotoLabelHasBeenClicked( int code )
{
	 if ( code == CardPhoto ) emit thePhotoLabelClicked();
}
