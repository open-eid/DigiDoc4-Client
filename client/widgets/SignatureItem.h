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

#include "widgets/Item.h"

class DigiDocSignature;

class SignatureItem final : public Item
{
	Q_OBJECT

public:
	explicit SignatureItem(DigiDocSignature s, QWidget *parent = nullptr);
	~SignatureItem() final;

	ria::qdigidoc4::WarningType getError() const;
	void initTabOrder(QWidget *item) final;
	bool isSelfSigned(const QString& cardCode) const;
	QWidget* lastTabWidget() final;

private:
	bool event(QEvent *event) final;
	bool eventFilter(QObject *o, QEvent *e) final;
	void init();
	void elideRole();

	struct Private;
	Private *ui;
};
