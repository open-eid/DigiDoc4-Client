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

#include "Item.h"

using namespace ria::qdigidoc4;

ItemList::ItemList(QWidget *parent)
	: QScrollArea(parent)
	, ui(new Ui::ItemList)
{
	ui->setupUi(this);
	ui->download->hide();
	ui->count->hide();
	ui->infoIcon->hide();
	ui->add->hide();
	connect(ui->add, &QToolButton::clicked, this, &ItemList::add);
	connect(this, &ItemList::idChanged, this, [this](const SslCertificate &cert){ this->cert = cert; });
}

ItemList::~ItemList()
{
	delete ui;
}

void ItemList::addHeader(const char *label)
{
	headerText = label;
	header = new QLabel(tr(label), this);
	header->setAlignment(Qt::AlignCenter);
	header->setStyleSheet(QStringLiteral("border-bottom: 1px solid #E7EAEF; color: #003168; font-size: 22px; padding: 16px 0px;"));
	ui->itemLayout->insertWidget(0, header);
	setTabOrder(this, header);
	setTabOrder(header, ui->header);
}

void ItemList::addTopWidget(QWidget *widget, QWidget *firstTab, QWidget *lastTab)
{
	topWidget = lastTab ? lastTab : widget;
	ui->itemLayout->insertWidget(1, widget);
	setTabOrder(ui->listHeader, firstTab ? firstTab : widget);
}

void ItemList::addHeaderWidget(Item *widget)
{
	addWidget(widget, ui->itemLayout->indexOf(ui->header), header);
	setTabOrder(widget->lastTabWidget(), ui->header);
}

void ItemList::addWidget(Item *widget, int index, QWidget *tabIndex)
{
	if(!tabIndex)
	{
		if(Item *prev = qobject_cast<Item*>(ui->itemLayout->itemAt(index - 1)->widget()))
			tabIndex = prev->lastTabWidget();
		else
			tabIndex = topWidget ? topWidget : ui->listHeader;
	}
	ui->itemLayout->insertWidget(index, widget);
	connect(widget, &Item::remove, this, &ItemList::remove);
	connect(this, &ItemList::idChanged, widget, &Item::idChanged);
	widget->stateChange(state);
	widget->idChanged(cert);
	widget->show();
	items.push_back(widget);
	widget->initTabOrder(tabIndex);
}

void ItemList::addWidget(Item *widget)
{
	addWidget(widget, ui->itemLayout->indexOf(ui->add));
}

void ItemList::changeEvent(QEvent* event)
{
	if (event->type() == QEvent::LanguageChange)
	{
		ui->retranslateUi(this);
		ui->listHeader->setText(tr(title));
		ui->add->setText(tr(addTitle));
		if(header)
			header->setText(tr(headerText));
		if(!ui->infoIcon->toolTip().isEmpty())
			setRecipientTooltip();
	}

	QFrame::changeEvent(event);
}

void ItemList::clear()
{
	ui->download->hide();
	ui->count->hide();

	if(header)
		header->deleteLater();
	header = nullptr;

	for(auto it = items.begin(); it != items.end(); it = items.erase(it))
		(*it)->deleteLater();
}

int ItemList::index(Item *item) const
{
	return items.indexOf(item);
}

void ItemList::init(ListType item, const char *header)
{
	ui->listHeader->setText(tr(title = header));

	switch(item)
	{
	case AddedAdresses:
		break;
	case ToAddAdresses:
		addTitle = QT_TR_NOOP("Add all");
		break;
	case ItemAddress:
		addTitle = QT_TR_NOOP("+ Add recipient");
		ui->infoIcon->load(QStringLiteral(":/images/icon_info.svg"));
		ui->infoIcon->show();
		setRecipientTooltip();
		break;
	case ItemFile:
		addTitle = QT_TR_NOOP("+ Add more files");
		break;
	case ItemSignature:
		ui->add->hide();
		break;
	}
	ui->add->setText(tr(addTitle));
}

void ItemList::remove(Item *item)
{
	if(int i = index(item); i != -1)
		emit removed(i);
}

void ItemList::removeItem(int row)
{
	if(row < items.size())
		items.takeAt(row)->deleteLater();
}

void ItemList::setRecipientTooltip()
{
	ui->infoIcon->setToolTip(tr("RECIPIENT_MESSAGE"));
}

void ItemList::stateChange( ContainerState state )
{
	this->state = state;
	ui->add->setVisible(state & (UnsignedContainer | UnsignedSavedContainer | UnencryptedContainer));

	for(auto item: items)
		item->stateChange(state);
}
