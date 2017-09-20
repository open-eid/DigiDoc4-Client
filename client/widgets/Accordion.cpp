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
	QWidget( parent ),
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
	ui->authBox->update( true,  "Isikutuvastamise sertifikaat", "Sertifikaat kehtib kuni 10. veebruar 2019",  "Muuda PIN1",  "<a href='#pin1-forgotten'><span style='color:black;'>Unustasid PIN1</span></a>",  "<a href='#pin1-cert'><span style='color:black;'>Vaata sertifikaadi detaile</span></a>", "Kala" );
	ui->signBox->update( false,  "Allkirjastamise sertifikaat",  "Sertifikaat on aegunud!",                    "Muuda PIN2",  "<a href='#pin2-forgotten'><span style='color:black;'>Unustasid PIN2</span></a>",  "<a href='#pin2-cert'><span style='color:black;'>Vaata sertifikaadi detaile</span></a>",  "PIN2 on blokeeritud, kuna PIN2 koodi on sisestatud 3 korda valesti. Tühista blokeering, et PIN2 taas kasutada." );
	ui->pukBox->update( true,    "PUK kood",                     "PUK kood asub Teie kooodiümbrikus",          "Muuda PUK" );

	ui->contentOtherData->update( false );
}

void Accordion::closeOtherSection( AccordionTitle* opened )
{
	openSection->closeSection();
	openSection = opened;
}

// Needed to setStyleSheet() take effect.
void Accordion::paintEvent( QPaintEvent * )
{
	QStyleOption opt;
	opt.init( this );
	QPainter p( this );
	style()->drawPrimitive( QStyle::PE_Widget, &opt, &p, this );
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
