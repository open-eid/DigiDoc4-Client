// SPDX-FileCopyrightText: Estonian Information System Authority
// SPDX-License-Identifier: LGPL-2.1-or-later

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
	void toggleCardPopup();
	void hideCardPopup();
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
	bool enumerated = false;
	const char *emptyStateText = QT_TR_NOOP("Reading card...");
};
