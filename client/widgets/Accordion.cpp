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

#include "XmlReader.h"
#include <common/SslCertificate.h>

Q_DECLARE_METATYPE(MobileStatus)

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

    ui->titleVerifyCert->init(true, tr("PIN/PUK CODES AND CERTIFICATES"), ui->contentVerifyCert);
    ui->titleOtherData->init(false, tr("REDIRECTION OF EESTI.EE E-MAIL"), ui->contentOtherData);
    ui->titleOtherEID->init(false, tr("MY OTHER eID's"), ui->contentOtherEID);

	connect(ui->titleVerifyCert, &AccordionTitle::opened, this, &Accordion::closeOtherSection);
	connect(ui->titleOtherData, &AccordionTitle::opened, this, &Accordion::closeOtherSection);
	connect(ui->titleOtherEID, &AccordionTitle::opened, this, &Accordion::closeOtherSection);

	connect(ui->authBox, &VerifyCert::changePinClicked, this, &Accordion::changePin1 );
	connect(ui->signBox, &VerifyCert::changePinClicked, this, &Accordion::changePin2 );
	connect(ui->pukBox, &VerifyCert::changePinClicked, this, &Accordion::changePuk );

	connect(ui->authBox, &VerifyCert::certDetailsClicked, this, &Accordion::certDetails );
	connect(ui->signBox, &VerifyCert::certDetailsClicked, this, &Accordion::certDetails );

	// Initialize PIN/PUK content widgets.
	ui->signBox->addBorders();

	clearOtherEID();
	
	// top | right | bottom | left
	QString otherIdStyle = "background-color: #ffffff; border: solid #DFE5E9; border-width: 1px %1px 0px %1px;";
	ui->mobID->setStyleSheet( otherIdStyle.arg("0") );
	ui->digiID->setStyleSheet( otherIdStyle.arg("1") );
	ui->otherID->setStyleSheet( otherIdStyle.arg("0") );
}

void Accordion::closeOtherSection(AccordionTitle* opened)
{
	openSection->closeSection();
	openSection = opened;

	idCheckOtherEIdNeeded( opened );
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
	QSmartCardData t = smartCard->data();

	ui->authBox->setVisible( !t.authCert().isNull() );
	if ( !t.authCert().isNull() )
		ui->authBox->update( QSmartCardData::Pin1Type, smartCard );

	ui->signBox->setVisible( !t.signCert().isNull() );
	if ( !t.signCert().isNull() )
		ui->signBox->update( QSmartCardData::Pin2Type, smartCard );

	ui->pukBox->update( QSmartCardData::PukType, smartCard );

	ui->otherID->update( "Teised eID-d");

	ui->titleOtherData->setVisible( !( t.version() == QSmartCardData::VER_USABLEUPDATER || t.authCert().subjectInfo( "O" ).contains( "E-RESIDENT" ) ) );
//	ui->titleOtherData->setDisabled( t.version() == QSmartCardData::VER_USABLEUPDATER || t.authCert().subjectInfo( "O" ).contains( "E-RESIDENT" ) );
}

void Accordion::changeEvent(QEvent* event)
{
	if (event->type() == QEvent::LanguageChange)
	{
		ui->retranslateUi(this);

		ui->titleVerifyCert->setText(tr("PIN/PUK CODES AND CERTIFICATES"));
		ui->titleOtherData->setText(tr("REDIRECTION OF EESTI.EE E-MAIL"));
		ui->titleOtherEID->setText(tr("MY OTHER eID's"));
	}

	QWidget::changeEvent(event);
}


void Accordion::changePin1( bool isForgotPin, bool isBlockedPin )
{
	emit changePin1Clicked ( isForgotPin, isBlockedPin );
}

void Accordion::changePin2( bool isForgotPin, bool isBlockedPin )
{
	emit changePin2Clicked ( isForgotPin, isBlockedPin );
}

void Accordion::changePuk( bool isForgotPuk, bool isBlockedPin )
{
	emit changePukClicked ( isForgotPuk );
}

void Accordion::certDetails( const QString &link )
{
	emit certDetailsClicked ( link );
}

void Accordion::clearOtherEID()
{
	// Set to default Certificate Info page
	updateOtherData( false );    // E-mail
	closeOtherSection( ui->titleVerifyCert );
    ui->titleVerifyCert->openSection();

	ui->mobID->hide();
	ui->digiID->hide();

	setProperty( "MOBILE_ID_STATUS", QVariant() );
}

void Accordion::updateMobileIdInfo()
{
	MobileStatus mobile = property("MOBILE_ID_STATUS").value<MobileStatus>();
	ui->mobID->updateMobileId(
		mobile.value( "MSISDN" ),
		mobile.value( "Operator" ),
		mobile.value( "Status" ),
		mobile.value( "MIDCertsValidTo" ) );

	ui->mobID->show();
}

void Accordion::updateDigiIdInfo()
{
	QVariant digiIdData = property("DIGI_ID_STATUS").value<QVariant>();

	ui->digiID->updateDigiId(
		"PA789456123",
		"01.01.2022",
		"Activ",
		"07.08.2022, 20:59:59" );

	ui->digiID->show();
}

void Accordion::idCheckOtherEIdNeeded( AccordionTitle* opened )
{
	// If user has not validated (entering valid PIN1) the EID status yet.
	if ( property("MOBILE_ID_STATUS").isNull() && opened == ui->titleOtherEID )
		emit checkOtherEID ();
}
