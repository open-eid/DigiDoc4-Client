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

#include "CardWidget.h"

#include <QtWidgets/QVBoxLayout>

CardPopup::CardPopup(const QVector<TokenData> &cache, QWidget *parent)
	: StyledWidget(parent)
{
	if(CardWidget *cardInfo = parent->findChild<CardWidget*>(QStringLiteral("cardInfo")))
	{
		move(cardInfo->mapTo(parent, cardInfo->rect().bottomLeft()) + QPoint(0, 2));
		setMinimumWidth(cardInfo->width());
	}
	setStyleSheet(QStringLiteral(
		"border: solid rgba(217, 217, 216, 0.45); "
		"border-width: 0px 2px 2px 1px; "
		"background-color: rgba(255, 255, 255, 0.95);"));
	QVBoxLayout *layout = new QVBoxLayout(this);
	layout->setContentsMargins(1, 0, 2, 2);

	for(const TokenData &token: cache)
	{
		auto cardWidget = new CardWidget(true, this);
		cardWidget->setCursor(QCursor(Qt::PointingHandCursor));
		cardWidget->update(token, true);
		connect(cardWidget, &CardWidget::selected, this, &CardPopup::activated);
		layout->addWidget(cardWidget);
	}
	adjustSize();
}
