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

#include "InfoStack.h"
#include "ui_InfoStack.h"
#include "Styles.h"

InfoStack::InfoStack(QWidget *parent) :
	StyledWidget(parent),
	ui(new Ui::InfoStack)
{
	ui->setupUi(this);
	ui->verifyCert->setFont( Styles::font( Styles::Condensed, 14 ) );

	QFont font = Styles::font( Styles::Regular, 16 );
	font.setWeight( QFont::DemiBold );
	ui->valueGivenNames->setFont( font );
	ui->valueSurname->setFont( font );
	ui->valuePersonalCode->setFont( font );
	ui->valueCitizenship->setFont( font );
	ui->valueSerialNumber->setFont( font );
	ui->valueExpiryDate->setFont( Styles::font( Styles::Regular, 16 ) );
}

InfoStack::~InfoStack()
{
	delete ui;
}

void InfoStack::clearPicture()
{
	ui->photo->clear();
}

void InfoStack::update(const QString &givenNames, const QString &surname, const QString &personalCode, const QString &citizenship, const QString &serialNumber, const QString &expiryDate )
{
	ui->valueGivenNames->setText(givenNames);
	ui->valueSurname->setText(surname);
	ui->valuePersonalCode->setText(personalCode);
	ui->valueCitizenship->setText(citizenship);
	if( serialNumber.isEmpty() )
	{
		ui->valueSerialNumber->setText(serialNumber);
	}
	else
	{
		ui->valueSerialNumber->setText(serialNumber + "  |");
	}
	ui->valueExpiryDate->setText(expiryDate);
}


void InfoStack::showPicture( const QPixmap &pixmap )
{
    ui->photo->setProperty( "PICTURE", pixmap );
	ui->photo->setPixmap( pixmap.scaled( 120, 150, Qt::IgnoreAspectRatio, Qt::SmoothTransformation ) );
}
