// SPDX-FileCopyrightText: Estonian Information System Authority
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "TokenData.h"
#include "widgets/StyledWidget.h"

class QSvgWidget;
namespace Ui {
class CardWidget;
}

class CardWidget final: public StyledWidget
{
	Q_OBJECT

public:
	explicit CardWidget(QWidget *parent = nullptr);
	explicit CardWidget(bool popup, QWidget *parent = nullptr);
	~CardWidget() final;

	TokenData token() const;
	void update(const TokenData &token, bool multiple);

signals:
	void selected(const TokenData &token);

private:
	bool event(QEvent *ev) final;

	Ui::CardWidget *ui;
	TokenData t;
	bool isPopup = false;
	bool isMultiple = false;
};
