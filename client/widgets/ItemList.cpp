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

#include "ItemList.h"
#include "ui_ItemList.h"

#include "Styles.h"
#include "dialogs/AddRecipients.h"
#include "widgets/AddressItem.h"
#include "widgets/SignatureItem.h"

#include <vector>
#include <QLabel>
#include <QLayoutItem>

using namespace ria::qdigidoc4;

ItemList::ItemList(QWidget *parent)
: QWidget(parent)
, ui(new Ui::ItemList)
, headerItems(1)
, state(UnsignedContainer)
{
	ui->setupUi(this);
	ui->findGroup->hide();
	ui->download->hide();	
}

ItemList::~ItemList()
{
	delete ui;
}

void ItemList::addHeader(const QString &label)
{
	header = new QLabel(label, this);
	header->setAlignment(Qt::AlignHCenter | Qt::AlignVCenter);
	header->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
	header->resize(415, 64);
	header->setFixedHeight(64);
	header->setFont( Styles::font(Styles::Regular, 20));
	header->setStyleSheet("border: solid rgba(217, 217, 216, 0.45);"
			"border-width: 0px 0px 1px 0px;");
	ui->itemLayout->insertWidget(0, header);
	headerItems++;
}

void ItemList::addHeaderWidget(StyledWidget *widget)
{
	ui->itemLayout->insertWidget(headerItems - 1, widget);
	widget->show();
	widget->stateChange(state);
	items.push_back(widget);
	headerItems++;
}

QString ItemList::addLabel() const
{
	switch(itemType)
	{
	case File: return "+ LISA VEEL FAILE";
	case Address: return "+ LISA ADRESSAAT";
	case ToAddAdresses: return "LISA KÕIK";
	default: return "";
	}
}

void ItemList::addressSearch()
{
	AddRecipients dlg(qApp->activeWindow());
	dlg.exec();
}

void ItemList::addWidget(StyledWidget *widget)
{
	ui->itemLayout->insertWidget(items.size() + headerItems, widget);
	widget->show();
	widget->stateChange(state);
	items.push_back(widget);
}

void ItemList::clear()
{
	ui->download->hide();	

	if(header)
	{
		header->close();
		delete header;
		header = nullptr;
		headerItems = ui->findGroup->isHidden() ? 1 : 2;
	}

	StyledWidget* widget;
	auto it = items.begin();
	while (it != items.end()) {
		widget = *it;
		it = items.erase(it);
		widget->close();
		delete widget;
	}
}

void ItemList::init(ItemType item, const QString &header, bool hideFind)
{
	itemType = item;
	ui->listHeader->setText(header);
	ui->listHeader->setFont( Styles::font(Styles::Regular, 20));

	if(hideFind)
	{
		ui->findGroup->hide();
		ui->listHeader->setStyleSheet("border: solid rgba(217, 217, 216, 0.45);"
			"border-width: 0px 0px 1px 0px;");
		headerItems = 1;
	}
	else
	{
		ui->btnFind->setFont(Styles::font(Styles::Condensed, 14));
		ui->txtFind->setFont(Styles::font(Styles::Regular, 12));
		ui->listHeader->setStyleSheet("border: none;");
		headerItems = 2;
	}

	if (item == Signature || item == AddedAdresses)
	{
		ui->add->hide();
	}
	else
	{
		ui->add->init(LabelButton::DeepCeruleanWithLochmara, addLabel(), itemType == File ? FileAdd : AddressAdd);
		ui->add->setFont(Styles::font(Styles::Condensed, 12));
	}

	if(item == Address)
		connect(ui->add, &LabelButton::clicked, this, &ItemList::addressSearch);
}

void ItemList::stateChange( ContainerState state )
{
	this->state = state;
	ui->add->setVisible(state & (UnsignedContainer | UnsignedSavedContainer | UnencryptedContainer));

	for(auto item: items)
	{
		item->stateChange(state);
	}
}
