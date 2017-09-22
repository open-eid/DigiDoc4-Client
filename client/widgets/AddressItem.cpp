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
#include "effects/ButtonHoverFilter.h"

using namespace ria::qdigidoc4;

AddressItem::AddressItem(ContainerState state, QWidget *parent)
: StyledWidget(parent)
, ui(new Ui::AddressItem)
{
	ui->setupUi(this);
	ui->name->setFont( Styles::font( Styles::Regular, 14, QFont::DemiBold ) );
	ui->code->setFont( Styles::font( Styles::Regular, 14 ) );
	ui->idType->setFont( Styles::font( Styles::Regular, 11 ) );
	ui->remove->installEventFilter( new ButtonHoverFilter( ":/images/icon_remove.svg", ":/images/icon_remove_hover.svg", this ) );
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
