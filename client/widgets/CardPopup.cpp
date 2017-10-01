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

#include "CardPopup.h"

#include <QVariant>

CardPopup::CardPopup( const QSmartCard *smartCard, QWidget *parent )
: StyledWidget(parent)
{
	QString selected = smartCard->data().data( QSmartCardData::Id ).toString();
	auto cards = smartCard->data().cards();

	resize( 385, cards.size() * 66 );
	move( 115, 66 );
	setStyleSheet( "border: solid rgba(217, 217, 216, 0.45); border-width: 0px 2px 2px 1px; background-color: rgba(255, 255, 255, 0.85);" );

	int i = 0;
	for( auto card: cards )
	{
		auto item = new QWidget( this );
		item->resize( this->width(), 65);
		item->move( 0, 1 + i * 66 );
		item->setStyleSheet( "QWidget:!hover { border:none; } QWidget:hover { border:none; background: qlineargradient(x1: 0, y1: 0, x2: 1, y2: 0, stop: 0 rgba(0, 118, 190, 0.15), stop: 1 rgba(255,255,255, 0.5));}" );
		
		auto cardWidget = new CardWidget( item );
		auto cardData = smartCard->cache()[card];
		if( cardData.isNull() )
		{
			cardData.reset( new QCardInfo( card ) );
		}
		cardWidget->move( 0, 0 );
		cardWidget->update( cardData );
		item->show();
		cardWidget->show();
		cardWidgets << cardWidget;
		i++;
	}
}

void CardPopup::update( const QSmartCard *smartCard )
{
	for( auto cardWidget: cardWidgets )
	{
		auto cardData = smartCard->cache()[cardWidget->id()];
		if( cardWidget->isLoading() && !cardData.isNull() )
		{
			cardWidget->update( cardData );
		}
	}
}