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
	bool eventFilter(QObject *o, QEvent *e) final;

	Ui::CardWidget *ui;
	TokenData t;
	bool isPopup = false;
	bool isMultiple = false;
};
