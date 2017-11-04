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
#include "effects/Overlay.h"
#include "widgets/AddressItem.h"


AddRecipients::AddRecipients(QWidget *parent) :
	QDialog(parent),
	ui(new Ui::AddRecipients)
{
	init();
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
	ui->rightPane->init(ria::qdigidoc4::ToAddAdresses, "Added recipients");
	ui->rightPane->setFont(Styles::font(Styles::Regular, 20));

	ui->fromCard->setFont(Styles::font(Styles::Condensed, 12));
	ui->fromFile->setFont(Styles::font(Styles::Condensed, 12));
	ui->fromHistory->setFont(Styles::font(Styles::Condensed, 12));

	ui->cancel->setFont(Styles::font(Styles::Condensed, 14));
	ui->confirm->setFont(Styles::font(Styles::Condensed, 14));

	connect(ui->confirm, &QPushButton::clicked, this, &AddRecipients::accept);
	connect(ui->cancel, &QPushButton::clicked, this, &AddRecipients::reject);
	connect(this, &AddRecipients::finished, this, &AddRecipients::close);



	AddressItem *add1 = new AddressItem(ria::qdigidoc4::ContainerState::UnsignedContainer, this);
	AddressItem *add2 = new AddressItem(ria::qdigidoc4::ContainerState::UnsignedContainer, this);
	add1->update("Aadu Aamer", "34511114231", "ID-kaat", AddressItem::Added);
	add2->update("Alma Tamm", "44510104561", "Digi-ID", AddressItem::Add);
	ui->leftPane->addWidget(add1);
	ui->leftPane->addWidget(add2);

	AddressItem *curr1 = new AddressItem(ria::qdigidoc4::ContainerState::UnsignedContainer, this);
	AddressItem *curr2 = new AddressItem(ria::qdigidoc4::ContainerState::UnsignedContainer, this);
	AddressItem *curr3 = new AddressItem(ria::qdigidoc4::ContainerState::UnsignedContainer, this);
	curr1->update( "Heino Liin", "34664778636", "ID-kaat", AddressItem::Remove );
	curr2->update( "Vello Karm", "44510104561", "ID-kaat", AddressItem::Remove );
	curr3->update( "Ivar Tuisk", "45643644331", "Digi-ID", AddressItem::Remove );
	ui->rightPane->addWidget( curr2 );
	ui->rightPane->addWidget( curr1 );
	ui->rightPane->addWidget( curr3 );
}

int AddRecipients::exec()
{
	Overlay overlay(parentWidget());
	overlay.show();
	auto rc = QDialog::exec();
	overlay.close();

	return rc;
}
