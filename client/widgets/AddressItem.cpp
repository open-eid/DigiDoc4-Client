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

#include "AddressItem.h"
#include "ui_AddressItem.h"
#include "Styles.h"

using namespace ria::qdigidoc4;

AddressItem::AddressItem(ContainerState state, QWidget *parent)
: ItemWidget(parent)
, ui(new Ui::AddressItem)
{
    ui->setupUi(this);
    ui->signatureInfo->setFont(Styles::instance().font(Styles::OpenSansRegular, 13));
    ui->remove->init(LabelButton::Mojo | LabelButton::AlabasterBackground, "Eemalda", SignatureRemove);
    setStyleSheet("border: solid #c8c8c8; border-width: 1px 0px 1px 0px; background-color: #fafafa; color: #000000; text-decoration: none solid rgb(0, 0, 0);");
}

AddressItem::~AddressItem()
{
    delete ui;
}

void AddressItem::stateChange(ContainerState state)
{
    if( state == UnencryptedContainer )
    {
        ui->remove->show();
    }
    else
    {
        ui->remove->hide();
    }
}
