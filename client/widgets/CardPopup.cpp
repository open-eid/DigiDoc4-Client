// SPDX-FileCopyrightText: Estonian Information System Authority
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "CardPopup.h"

#include "CardWidget.h"

#include <QtWidgets/QVBoxLayout>

CardPopup::CardPopup(const QVector<TokenData> &cache, QWidget *parent)
	: StyledWidget(parent)
{
	setObjectName(QStringLiteral("CardPopup"));
	setStyleSheet(QStringLiteral("#CardPopup {"
		"border: solid rgba(217, 217, 216, 0.45); "
		"border-width: 0px 2px 2px 1px; "
		"background-color: rgba(255, 255, 255, 0.95); }"));
	if(auto *cardInfo = parent->findChild<CardWidget*>(QStringLiteral("cardInfo")))
	{
		move(cardInfo->mapTo(parent, cardInfo->rect().bottomLeft()) + QPoint(0, 2));
		setMinimumWidth(cardInfo->width());
	}
	auto *layout = new QVBoxLayout(this);
	layout->setContentsMargins(1, 0, 2, 2);

	for(const TokenData &token: cache)
	{
		auto *cardWidget = new CardWidget(true, this);
		cardWidget->setCursor(QCursor(Qt::PointingHandCursor));
		cardWidget->update(token, true);
		connect(cardWidget, &CardWidget::selected, this, &CardPopup::activated);
		layout->addWidget(cardWidget);
	}
	adjustSize();
}
