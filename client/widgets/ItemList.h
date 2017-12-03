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

#include "common_enums.h"
#include "widgets/Item.h"

#include <functional>
#include <QWidget>

namespace Ui {
class ItemList;
}

class QLabel;
class QSvgWidget;

class ItemList : public QWidget
{
	Q_OBJECT

public:
	explicit ItemList(QWidget *parent = nullptr);
	virtual ~ItemList();

	void init(ria::qdigidoc4::ItemType itemType, const QString &header);
	void addHeader(const QString &label);
	void addHeaderWidget(Item *widget);
	void addWidget(Item *widget);
	virtual void clear();
	ria::qdigidoc4::ContainerState getState() const;
	bool hasItem(std::function<bool(Item* const)> cb);
	virtual void removeItem(int row);
	void stateChange(ria::qdigidoc4::ContainerState state);

signals:
	void addAll();
	void addItem(int code);
	void addressSearch();
	void idChanged(const QString& cardCode, const QString& mobileCode);
	void keysSelected(QList<Item *> keys);
	void removed(int row);
	void search(const QString &term);

public slots:
	void details(const QString &id);

protected slots:
	virtual void remove(Item *item);
	
protected:
	int index(Item *item) const;

	Ui::ItemList* ui;
	std::vector<Item*> items;
	ria::qdigidoc4::ContainerState state;

protected:
	void changeEvent(QEvent* event) override;

private:
	QString addLabel();
	void addWidget(Item *widget, int index);
	void focusEvent(int eventType);
	void setRecipientTooltip();

	QLabel *header = nullptr;
	int headerItems;
	QString idCode;
	QSvgWidget* infoIcon = nullptr;
	QSvgWidget* infoHoverIcon = nullptr;
	ria::qdigidoc4::ItemType itemType;
	QString mobileCode;
	QString headerText;
	QString listText;

	friend class AddRecipients;
};
