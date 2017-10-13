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

#include "NoOtherId.h"
#include "ui_NoOtherId.h"
#include "Styles.h"

NoOtherId::NoOtherId( QWidget *parent ) :
	StyledWidget( parent ),
    ui( new Ui::NoOtherId )
{
	ui->setupUi(this);

	QFont font14 = Styles::font( Styles::Regular, 14 );
	QFont font18 = Styles::font( Styles::Regular, 18 );
	
	ui->lblName->setFont( font18 );
	ui->lblInfo->setFont( font14 );
	ui->lblMobileID->setFont( font14 );
	ui->lblDigiID->setFont( font14 );
	ui->lblSmartID->setFont( font14 );

	// top | right | bottom | left
	this->setStyleSheet( "background-color: #ffffff; border: solid #DFE5E9; border-width: 1px 0px 0px 1px;" );
}

NoOtherId::~NoOtherId()
{
    delete ui;
}

void NoOtherId::update( const QString &lblName )
{
	ui->lblName->setText( lblName );
    QString decoration = "style='color: #006EB5; text-decoration: none;'";

	ui->lblMobileID->setText( "<a href='http://mobiil.id.ee'><span " + decoration + ">Mobiili-ID</span></a>" );
	ui->lblDigiID->setText( "<a href='https://www.id.ee/?lang=et&id=34178/'><span " + decoration + ">Digi-ID</span></a>" );
	ui->lblSmartID->setText( "<a href='https://www.smart-id.com/et/'><span " + decoration + ">SmartID</span></a>" );
}
