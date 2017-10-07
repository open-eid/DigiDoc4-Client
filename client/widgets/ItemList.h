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
#include "widgets/StyledWidget.h"

#include <QWidget>

namespace Ui {
class ItemList;
}

class QLabel;

class ItemList : public QWidget
{
	Q_OBJECT

public:
	enum ItemType {
		File,
		Signature,
		Address,
		ToAddAdresses,
		AddedAdresses,
	};

	explicit ItemList(QWidget *parent = nullptr);
	virtual ~ItemList();

	void init(ItemType itemType, const QString &header, bool hideFind = true);
	void add(int code);
	void addHeader(const QString &label);
	void addHeaderWidget(StyledWidget *widget);
	void addWidget(StyledWidget *widget);
	void addFile( const QString& file );
	void clear();
	void stateChange(ria::qdigidoc4::ContainerState state);

signals:
	void addItem(int code);

protected:
	void showDownload();

	std::vector<StyledWidget*> items;
	ria::qdigidoc4::ContainerState state;

private:
	QString addLabel() const;
	QString anchor() const;

	Ui::ItemList* ui;
	QLabel *header = nullptr;
	int headerItems;
	ItemType itemType;
};
