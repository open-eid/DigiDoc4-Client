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

#include "Accordion.h"
#include "ui_Accordion.h"

Accordion::Accordion( QWidget *parent ) :
	StyledWidget( parent ),
	ui( new Ui::Accordion ),
	openSection( nullptr )
{
	ui->setupUi( this );
}

Accordion::~Accordion()
{
	delete ui;
}

void Accordion::init()
{
	// Initialize accordion.
	openSection = ui->titleVerifyCert;
    
	connect( ui->contentOtherData, &OtherData::checkEMailClicked, this, [this](){ emit checkEMail(); } );
	connect( ui->contentOtherData, &OtherData::activateEMailClicked, this, [this](){ emit activateEMail(); } );
    
	ui->titleOtherEID->init( this,   false, "TEISED eID’D", ui->contentOtherEID );
	ui->titleVerifyCert->init( this, true,  "PIN/PUK KOODID JA SERTIFIKAATIDE KONTROLL", ui->contentVerifyCert );
	ui->titleOtherData->init( this,  false, "MUUD ANDMED", ui->contentOtherData );

	// Initialize PIN/PUK content widgets.
	ui->signBox->addBorders();

	ui->contentOtherData->update( false );
}

void Accordion::closeOtherSection( AccordionTitle* opened )
{
	openSection->closeSection();
	openSection = opened;
}

void Accordion::updateOtherData( bool activate, const QString &eMail, const quint8 &errorCode )
{
	ui->contentOtherData->update( activate, eMail, errorCode );
}

QString Accordion::getEmail()
{
	return ui->contentOtherData->getEmail();
}

void Accordion::setFocusToEmail()
{
	ui->contentOtherData->setFocusToEmail();
}

void Accordion::updateInfo( const QSmartCard *smartCard )
{
	ui->authBox->update( QSmartCardData::Pin1Type, smartCard );
	ui->signBox->update( QSmartCardData::Pin2Type, smartCard );
	ui->pukBox->update( QSmartCardData::PukType, smartCard );
}
