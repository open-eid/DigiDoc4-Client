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

#include "widgets/AddressItem.h"

#include <QDialog>
#include <QMap>


namespace Ui {
class AddRecipients;
}

class AddRecipients : public QDialog
{
	Q_OBJECT

public:
	explicit AddRecipients(const std::vector<Item*>& items, QWidget *parent = 0);
	~AddRecipients();

	int exec() override;

protected:
	void setAddressItems(const std::vector<Item*>& items);
	void enableRecipientFromCard();
	void addRecipientFromCard();
	void addRecipientToRightList(Item* toAdd);
	void addAllRecipientFromLeftPane();
	void removeRecipientFromRightList(Item *toRemove);

private:
	void init();

	Ui::AddRecipients *ui;
	QMap<QString, AddressItem *> leftList;
	QStringList rightList;
};
