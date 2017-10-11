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
#include "crypto/CryptoDoc.h"
#include "effects/ButtonHoverFilter.h"

#include <common/SslCertificate.h>

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
	ui->add->setFont(Styles::font(Styles::Condensed, 12));
	ui->added->setFont(Styles::font(Styles::Condensed, 12));

	ui->add->hide();
	ui->added->hide();
}

AddressItem::AddressItem(const CKey &key, ContainerState state, QWidget *parent)
: AddressItem(state, parent)
{
	QString name = !key.cert.subjectInfo("GN").isEmpty() && !key.cert.subjectInfo("SN").isEmpty() ?
			key.cert.subjectInfo("GN").value(0) + " " + key.cert.subjectInfo("SN").value(0) :
			key.cert.subjectInfo("CN").value(0);

	QString type;
	switch (SslCertificate(key.cert).type())
	{
	case SslCertificate::DigiIDType:
		type = "Digi-ID";
		break;
	case SslCertificate::EstEidType:
		type = "ID-kaart";
		break;
	case SslCertificate::MobileIDType:
		type = "Mobiil-ID";
		break;
	default:
		type = "UnknownType";
		break;
	}

	update(name, key.cert.subjectInfo("serialNumber").value(0), type, AddressItem::Remove);
}

AddressItem::~AddressItem()
{
	delete ui;
}

void AddressItem::update(const QString& name, const QString& code, const QString& type, ShowToolButton show)
{
	ui->name->setText( name );
	ui->code->setText( code );
	ui->idType->setText( type );

	ui->added->setEnabled(false);

	if(show == Added)
	{
		setStyleSheet(
					"border-bottom: 2px solid rgba(217, 217, 216, 0.45);"
					"background-color: #f0f0f0;"
					);
		ui->name->setStyleSheet("color: #75787B;");
		ui->code->setStyleSheet("color: #75787B;");
	}
	else
	{
		setStyleSheet(
					"border-bottom: 2px solid rgba(217, 217, 216, 0.45);"
					"background-color: #ffffff;"
					);
		ui->name->setStyleSheet("color: #363739;");
		ui->code->setStyleSheet("color: #363739;");
	}


	switch (show) {
	case Remove:
		ui->remove->show();
		ui->add->hide();
		ui->added->hide();
		break;
	case Add:
		ui->remove->hide();
		ui->add->show();
		ui->added->hide();
		break;
	case Added:
		ui->remove->hide();
		ui->add->hide();
		ui->added->show();
		break;
	default:
		ui->remove->hide();
		ui->add->hide();
		ui->added->hide();
		break;
	}
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
