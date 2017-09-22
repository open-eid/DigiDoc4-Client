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

#include "SignatureItem.h"
#include "ui_SignatureItem.h"
#include "effects/ButtonHoverFilter.h"
#include "Styles.h"

using namespace ria::qdigidoc4;

SignatureItem::SignatureItem(ContainerState state, QWidget *parent)
: StyledWidget(parent)
, ui(new Ui::SignatureItem)
{
	ui->setupUi(this);
	ui->name->setFont( Styles::font( Styles::Regular, 14, QFont::DemiBold ) );
	ui->status->setFont( Styles::font(Styles::Regular, 14) );
	ui->idSignTime->setFont( Styles::font(Styles::Regular, 11) );
	ui->remove->installEventFilter( new ButtonHoverFilter( ":/images/icon_remove.svg", ":/images/icon_remove_hover.svg", this ) );
}

SignatureItem::~SignatureItem()
{
	delete ui;
}

void SignatureItem::stateChange(ContainerState state)
{
}
