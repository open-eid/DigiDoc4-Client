// SPDX-FileCopyrightText: Estonian Information System Authority
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "common_enums.h"

#include "SslCertificate.h"

#include <QScrollArea>

namespace Ui {
class ItemList;
}

class Item;
class QLabel;

class ItemList : public QScrollArea
{
	Q_OBJECT

public:
	enum ListType : unsigned char {
		ItemFile,
		ItemSignature,
		ItemAddress,
		ToAddAdresses,
		AddedAdresses,
	};

	explicit ItemList(QWidget *parent = {});
	~ItemList() override;

	void init(ListType itemType, const char *header);
	void addHeader(const char *label);
	void addHeaderWidget(Item *widget);
	void addTopWidget(QWidget *widget, QWidget *firstTab = {}, QWidget *lastTab = {});
	void addWidget(Item *widget);
	virtual void clear();
	virtual void removeItem(int row);
	virtual void stateChange(ria::qdigidoc4::ContainerState state);

signals:
	void add();
	void addItem(int code);
	void idChanged(const SslCertificate &cert);
	void keysSelected(QList<Item *> keys);
	void removed(int row);

protected:
	void changeEvent(QEvent* event) override;
	int index(Item *item) const;
	virtual void remove(Item *item);

	Ui::ItemList* ui;
	ria::qdigidoc4::ContainerState state = ria::qdigidoc4::UnencryptedContainer;

private:
	void addWidget(Item *widget, int index, QWidget *tabIndex = {});
	void setRecipientTooltip();

	QList<Item*> items;
	QLabel *header = nullptr;
	QWidget *topWidget = nullptr;
	const char *title = "";
	const char *addTitle = "";
	const char *headerText = "";
	SslCertificate cert;

	friend class AddRecipients;
};
