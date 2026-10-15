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

#pragma once

#include "QSmartCard.h"
#include "TokenData.h"

#include <QWidget>

namespace Ui {
class MyEidInfo;
}

class CardPopup;

class MyEidInfo final: public QWidget
{
	Q_OBJECT

public:
	explicit MyEidInfo( QWidget *parent = nullptr );
	~MyEidInfo() final;

	QSmartCardData cardData() const;
	void changePin(QSmartCardData::PinType type, QSmartCard::PinAction action);

Q_SIGNALS:
	void dataChanged(const QSmartCardData &data);

private:
	void changeEvent(QEvent* event) final;
	bool eventFilter(QObject *watched, QEvent *event) final;
	void hideEvent(QHideEvent *event) final;
	void showEvent(QShowEvent *event) final;
	void selectCard(const TokenData &token);
	void refresh();
	void updateEmptyState(bool enumerationComplete = false);
	QList<TokenData> otherCards() const;
	void updateSelector();
	void updateCards(QList<TokenData> tokens);
	void updateData();
	void updateLabels();

	Ui::MyEidInfo *ui;
	CardPopup *cardPopup = nullptr;

	QList<TokenData> cards;
	TokenData selectedToken;
	QSmartCardData currentData;
	quint64 request = 0;
	bool readFailed = false;
	const char *emptyStateText = QT_TR_NOOP("Reading card...");
};
