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


#include "AddRecipients.h"
#include "ui_AddRecipients.h"

#include "common_enums.h"
#include "Styles.h"
#include "client/Application.h"
#include "client/QSigner.h"
#include "common/SslCertificate.h"
#include "common/TokenData.h"
#include "effects/Overlay.h"

#include <QDebug>

AddRecipients::AddRecipients(const std::vector<Item *> &items, QWidget *parent) :
	QDialog(parent),
	ui(new Ui::AddRecipients),
  leftList(),
  rightList()
{
	init();
	setAddressItems(items);
}

AddRecipients::~AddRecipients()
{
	delete ui;
}

void AddRecipients::init()
{
	ui->setupUi(this);
	setWindowFlags( Qt::Dialog | Qt::FramelessWindowHint );
	setWindowModality( Qt::ApplicationModal );

	ui->leftPane->init(ria::qdigidoc4::ToAddAdresses, "Add recipients", false);
	ui->leftPane->setFont(Styles::font(Styles::Regular, 20));
	ui->rightPane->init(ria::qdigidoc4::AddedAdresses, "Added recipients");
	ui->rightPane->setFont(Styles::font(Styles::Regular, 20));

	ui->fromCard->setFont(Styles::font(Styles::Condensed, 12));
	ui->fromFile->setFont(Styles::font(Styles::Condensed, 12));
	ui->fromHistory->setFont(Styles::font(Styles::Condensed, 12));

	ui->cancel->setFont(Styles::font(Styles::Condensed, 14));
	ui->confirm->setFont(Styles::font(Styles::Condensed, 14));

	connect(ui->confirm, &QPushButton::clicked, this, &AddRecipients::accept);
	connect(ui->cancel, &QPushButton::clicked, this, &AddRecipients::reject);
	connect(this, &AddRecipients::finished, this, &AddRecipients::close);

	connect(ui->leftPane, &ItemList::addAll, this, &AddRecipients::addAllRecipientFromLeftPane );
	connect(ui->rightPane, &ItemList::removed, ui->rightPane, &ItemList::removeItem );

	connect(ui->fromCard, &QPushButton::clicked, this, &AddRecipients::addRecipientFromCard);
	connect( qApp->signer(), &QSigner::authDataChanged, this, &AddRecipients::enableRecipientFromCard );
	enableRecipientFromCard();
}

void AddRecipients::setAddressItems(const std::vector<Item *> &items)
{
	for(Item *item :items)
	{
		AddressItem *leftItem = new AddressItem((static_cast<AddressItem *>(item))->getKey(),  ria::qdigidoc4::UnencryptedContainer, ui->leftPane);
		QString friendlyName = SslCertificate(leftItem->getKey().cert).friendlyName();

		// Add to left pane
		leftList.insert(friendlyName, leftItem);
		ui->leftPane->addWidget(leftItem);

		leftItem->showButton(AddressItem::Added);
		connect(leftItem, &AddressItem::add, this, &AddRecipients::addRecipientToRightList );

		// Add to right pane
		addRecipientToRightList(leftItem);
	}
}

void AddRecipients::enableRecipientFromCard()
{
	ui->fromCard->setDisabled( qApp->signer()->tokenauth().cert().isNull() );
}

void AddRecipients::addRecipientToRightList(Item *toAdd)
{
	AddressItem *leftItem = static_cast<AddressItem *>(toAdd);
	AddressItem *rightItem = new AddressItem(leftItem->getKey(),  ria::qdigidoc4::UnencryptedContainer, ui->leftPane);

	rightList.append(SslCertificate(leftItem->getKey().cert).friendlyName());
	ui->rightPane->addWidget(rightItem);
	rightItem->showButton(AddressItem::Remove);

	connect(rightItem, &AddressItem::remove, this, &AddRecipients::removeRecipientFromRightList );
	leftItem->showButton(AddressItem::Added);
}

void AddRecipients::addAllRecipientFromLeftPane()
{
	for(auto it = leftList.begin(); it != leftList.end(); ++it )
	{
		if(!rightList.contains(it.key()))
		{
			addRecipientToRightList(it.value());
		}
	}
}

void AddRecipients::removeRecipientFromRightList(Item *toRemove)
{
	AddressItem *rightItem = static_cast<AddressItem *>(toRemove);
	QString friendlyName = SslCertificate(rightItem->getKey().cert).friendlyName();

	auto it = leftList.find(friendlyName);
	if(it != leftList.end())
	{
		it.value()->showButton(AddressItem::Add);
		rightList.removeAll(friendlyName);
	}
}


void AddRecipients::addRecipientFromCard()
{
	QList<QSslCertificate> cetrs;

	cetrs << qApp->signer()->tokenauth().cert();
	for(QSslCertificate& cert : cetrs)
	{
		QString friendlyName = SslCertificate(cert).friendlyName();
		if(!leftList.contains(friendlyName) )
		{
			AddressItem *leftItem = new AddressItem(CKey(cert),  ria::qdigidoc4::UnencryptedContainer, ui->leftPane);

			leftList.insert(friendlyName, leftItem);
			ui->leftPane->addWidget(leftItem);

			if(rightList.contains(friendlyName))
			{
				leftItem->showButton(AddressItem::Added);
			}
			else
			{
				leftItem->showButton(AddressItem::Add);
			}

			connect(leftItem, &AddressItem::add, this, &AddRecipients::addRecipientToRightList );
		}
	}
}


int AddRecipients::exec()
{
	Overlay overlay(parentWidget());
	overlay.show();
	auto rc = QDialog::exec();
	overlay.close();

	return rc;
}
