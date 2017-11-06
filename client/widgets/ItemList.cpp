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

#include <QLabel>

using namespace ria::qdigidoc4;


ItemList::ItemList(QWidget *parent)
: QWidget(parent)
, ui(new Ui::ItemList)
, state(UnsignedContainer)
, headerItems(1)
{
	ui->setupUi(this);
	ui->findGroup->hide();
	ui->download->hide();
	connect(this, &ItemList::idChanged, [this](const QString &code, const QString &mobile){idCode = code; mobileCode = mobile;});
}

ItemList::~ItemList()
{
	delete ui;
}

void ItemList::addHeader(const QString &label)
{
	headerText = label;

	header = new QLabel(tr(qPrintable(label)), this);
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

void ItemList::addHeaderWidget(Item *widget)
{
	addWidget(widget, headerItems - 1);
	headerItems++;
}

void ItemList::changeEvent(QEvent* event)
{
	if (event->type() == QEvent::LanguageChange)
	{
		ui->retranslateUi(this);

		ui->listHeader->setText(tr(qPrintable(listText)));

		if(header != nullptr)
			header->setText(tr(qPrintable(headerText)));

		if(ui->add->isVisible())
			ui->add->setText(tr(qPrintable(addLabel())));
	}

	QWidget::changeEvent(event);
}

QString ItemList::addLabel()
{
	switch(itemType)
	{
	case ItemFile: return tr("Add more files");
	case ItemAddress: return tr("Add addressee");
	case ToAddAdresses: return tr("Add all");
	default: return "";
	}
}

void ItemList::addressSearch()
{
	AddRecipients dlg(items, qApp->activeWindow());
	dlg.exec();
}

void ItemList::addWidget(Item *widget, int index)
{
	ui->itemLayout->insertWidget(index, widget);
	connect(widget, &Item::remove, this, &ItemList::remove);
	connect(this, &ItemList::idChanged, widget, &Item::idChanged);
	widget->stateChange(state);
	widget->idChanged(idCode, mobileCode);
	widget->show();
	items.push_back(widget);
}

void ItemList::addWidget(Item *widget)
{
	addWidget(widget, items.size() + headerItems);
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

	Item* widget;
	auto it = items.begin();
	while (it != items.end()) {
		widget = *it;
		it = items.erase(it);
		widget->close();
		delete widget;
	}
}

void ItemList::details(const QString &id)
{
	for(auto item: items)
	{
		if(item->id() == id)
			emit item->details();
	}
}

ContainerState ItemList::getState() { return state; }

int ItemList::index(Item *item) const
{
	auto it = std::find(items.begin(), items.end(), item);
	if(it != items.end())
		return std::distance(items.begin(), it);

	return -1;
}

void ItemList::init(ItemType item, const QString &header, bool hideFind)
{
	itemType = item;
	ui->listHeader->setText(tr(qPrintable(header)));
	listText = header;
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
		ui->findGroup->show();
		headerItems = 2;
	}

	if (itemType == ItemSignature || item == AddedAdresses)
	{
		ui->add->hide();
	}
	else
	{
		ui->add->init(LabelButton::DeepCeruleanWithLochmara, addLabel(), itemType == ItemFile ? FileAdd : AddressAdd);
		ui->add->setFont(Styles::font(Styles::Condensed, 12));
	}

	if(itemType == ItemAddress)
	{
		ui->add->disconnect();
		connect(ui->add, &LabelButton::clicked, this, &ItemList::addressSearch);
	}
	else if(itemType == ToAddAdresses)
	{
		connect(ui->add, &LabelButton::clicked, this, &ItemList::addAll);
	}
}

void ItemList::remove(Item *item)
{
	int i = index(item);
	if(i != -1)
		emit removed(i);
}

void ItemList::removeItem(int row)
{
	if(items.size() < row)
		return;

	auto item = items[row];
	item->close();
	items.erase(items.begin()+row);
}

void ItemList::stateChange( ContainerState state )
{
	this->state = state;
	ui->add->setVisible(state & (SignatureContainers | UnencryptedContainer));

	for(auto item: items)
	{
		item->stateChange(state);
	}
}
