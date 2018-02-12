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


#define BORDER_WIDTH 2
#define ROW_HEIGHT 66
#define WIDTH 365
#define X_POSITION 110

CardPopup::CardPopup(const TokenData &token, const QMap<QString, QSharedPointer<QCardInfo>> &cache, QWidget *parent)
: StyledWidget(parent)
{
	auto cards = token.cards();

	resize( WIDTH, (cards.size() - 1) * ROW_HEIGHT + ( (cards.size() > 1) ? BORDER_WIDTH : 0 ) );
	move( X_POSITION, ROW_HEIGHT );
	setStyleSheet( "border: solid rgba(217, 217, 216, 0.45); border-width: 0px 2px 2px 1px; background-color: rgba(255, 255, 255, 0.95);" );

	int i = 0;
	for( auto card: cards )
	{
		if( card == token.card() )
			continue;
		auto item = new QWidget( this );
		item->resize( WIDTH - 2, ROW_HEIGHT - 1);
		item->move( 0, 1 + i * ROW_HEIGHT );
		item->setStyleSheet( "QWidget:!hover { border:none; } QWidget:hover { border:none; background: qlineargradient(x1: 0, y1: 0, x2: 1, y2: 0, stop: 0 rgba(0, 118, 190, 0.15), stop: 1 rgba(255,255,255, 0.5));}" );
		
		auto cardWidget = new CardWidget( card, item );
		auto cardData = cache[card];
		if( cardData.isNull() )
		{
			cardData.reset(new QCardInfo);
			cardData->id = card;
		}
		else
		{
			Styles::cachedPicture( cardData->id, {cardWidget} );
		}
		cardWidget->move( 0, 0 );
		cardWidget->update( cardData );
		void activated( const QString &card );
		connect( cardWidget, &CardWidget::selected, this, &CardPopup::activated );

		cardWidgets << cardWidget;
		item->show();
		cardWidget->show();
		i++;
	}
}

void CardPopup::update(const QMap<QString, QSharedPointer<QCardInfo>> &cache)
{
	for( auto cardWidget: cardWidgets )
	{
		auto cardData = cache[cardWidget->id()];
		if( cardWidget->isLoading() && !cardData.isNull() )
		{
			cardWidget->update( cardData );
			Styles::cachedPicture( cardData->id, {cardWidget} );
		}
	}
}
