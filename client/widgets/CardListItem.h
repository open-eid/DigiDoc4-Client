// SPDX-FileCopyrightText: Estonian Information System Authority
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "TokenData.h"

#include <QtWidgets/QAbstractButton>

namespace Ui { class CardListItem; }

class CardListItem final: public QAbstractButton
{
	Q_OBJECT

public:
	explicit CardListItem(QWidget *parent = nullptr);
	~CardListItem() final;

	TokenData token() const;
	void update(const TokenData &token, bool selected, bool usable);

signals:
	void selected(const TokenData &token);

protected:
	void paintEvent(QPaintEvent *ev) final;
	void changeEvent(QEvent *ev) final;

private:
	Ui::CardListItem *ui;
	TokenData t;
};
