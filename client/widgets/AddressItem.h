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

#include "crypto/CryptoDoc.h"
#include "widgets/Item.h"

namespace Ui {
class AddressItem;
}

class AddressItem : public Item
{
	Q_OBJECT

public:
	enum ShowToolButton
	{
		None,
		Remove,
		Add,
		Added
	};

	explicit AddressItem(ria::qdigidoc4::ContainerState state, QWidget *parent = nullptr, bool showIcon = false);
	explicit AddressItem(const CKey &k, ria::qdigidoc4::ContainerState state, QWidget *parent = nullptr);
	~AddressItem();

	void idChanged(const QString& cardCode, const QString& mobileCode) override;
	void stateChange(ria::qdigidoc4::ContainerState state) override;
	void update(const QString& name, const QString& code, const QString &type, ShowToolButton show);

protected:
	void mouseReleaseEvent(QMouseEvent *event) override;
	void changeEvent(QEvent* event) override;

private:
	Ui::AddressItem *ui;

	QString code;
	CKey key;
    bool isYou;
    QString typeText;
};
