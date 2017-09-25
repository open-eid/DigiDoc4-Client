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

#include "VerifyCert.h"
#include "ui_VerifyCert.h"
#include "Styles.h"

#include <common/PinDialog.h>
#include <common/DateTime.h>
#include <common/SslCertificate.h>

#include <QtCore/QTextStream>
#include <QMessageBox>

VerifyCert::VerifyCert( QWidget *parent ) :
	StyledWidget( parent ),
	ui( new Ui::VerifyCert ),
	isValidCert( false ),
	isBlockedPin( false )
{
	ui->setupUi( this );

	connect( ui->changePIN, &QPushButton::clicked, this, &VerifyCert::processClickedBtn );

	ui->name->setFont( Styles::font( Styles::Regular, 18, QFont::Bold  ) );
	ui->validUntil->setFont( Styles::font( Styles::Regular, 14 ) );
	ui->error->setFont( Styles::font( Styles::Regular, 12, QFont::DemiBold ) );
	QFont regular12 = Styles::font( Styles::Regular, 12 );
	ui->forgotPinLink->setFont( regular12 );
	ui->details->setFont( regular12 );
	ui->changePIN->setFont( Styles::font( Styles::Condensed, 14 ) );
	borders = " border: solid #DFE5E9; border-width: 1px 0px 0px 0px; ";

//	connect( ui->changePIN, &QPushButton::clicked, this, [this](){ emit changePinClicked(); } );
}

VerifyCert::~VerifyCert()
{
	delete ui;
}

void VerifyCert::addBorders()
{
	// Add top, right and left border shadows
	borders = " border: solid #DFE5E9; border-width: 1px 1px 0px 1px; ";
}

void VerifyCert::update( QSmartCardData::PinType type, const QSmartCard *smartCard )
{
    this->pinType = type;
    this->smartCard = smartCard;
	QSmartCardData t = smartCard->data();
    
    SslCertificate c = ( type == QSmartCardData::Pin1Type ) ? t.authCert() : t.signCert();
	this->isValidCert = c.isValid();    // put 0 for testing Aegunud Sertifikaate.
	this->isBlockedPin = (t.retryCount( type ) == 0) ? true : false; // put 1 for testing Tühista Blokeering.

	QString name;
	QString validUntil;
	QString changeBtn;
	QString forgotPinText;
	QString detailsText;
	QString error;
	QString txt;
	QTextStream cert( &txt );

	if( !isValidCert )
	{
		cert << tr("Sertifikaat on aegunud!");
	}
	else
		cert << "Sertifikaat <span style='color: #37a447'>kehtib</span> kuni "
			<< DateTime(c.expiryDate().toLocalTime()).formatDate("dd. MMMM yyyy");

	switch( type )
	{
		case QSmartCardData::Pin1Type:
			name = "Isikutuvastamise sertifikaat";
			changeBtn = ( !isValidCert ) ? "UUENDA SERTIFIKAAT" : ( isBlockedPin ) ? "TÜHISTA BLOKEERING" : "MUUDA PIN1";
			forgotPinText = "<a href='#pin1-forgotten'><span style='color:black;'>Unustasid PIN1 koodi?</span></a>";
			detailsText = ( isValidCert ) ? "<a href='#pin1-cert'><span style='color:black;'>Vaata sertifikaadi detaile</span></a>" : "";
			error = ( !isValidCert ) ? "PIN1 ei saa kasutada, kuna sertifikaat on aegunud. Uuenda sertifikaat, et PIN1 taas kasutada." :
					( isBlockedPin ) ? "PIN1 on blokeeritud, kuna PIN1 koodi on sisestatud 3 korda valesti. Tühista blokeering, et PIN1 taas kasutada." :
					"";
			break;
		case QSmartCardData::Pin2Type:
			name = "Allkirjastamise sertifikaat";
			changeBtn = ( !isValidCert ) ? "UUENDA SERTIFIKAAT" : ( isBlockedPin ) ? "TÜHISTA BLOKEERING" : "MUUDA PIN2";
			forgotPinText = "<a href='#pin2-forgotten'><span style='color:black;'>Unustasid PIN2 koodi?</span></a>";
			detailsText = ( isValidCert ) ? "<a href='#pin2-cert'><span style='color:black;'>Vaata sertifikaadi detaile</span></a>" : "";
			error = ( !isValidCert ) ? "PIN2 ei saa kasutada, kuna sertifikaat on aegunud. Uuenda sertifikaat, et PIN2 taas kasutada." :
					( isBlockedPin ) ? "PIN2 on blokeeritud, kuna PIN2 koodi on sisestatud 3 korda valesti. Tühista blokeering, et PIN2 taas kasutada." : 
					"";
			break;
		case QSmartCardData::PukType:
			name = "PUK kood";
			txt = ""; // "PUK kood asub Teie kooodiümbrikus";
			changeBtn = "MUUDA PUK"; 
			forgotPinText = "<a href='#pin1-forgotten'><span style='color:black;'>Unustasid PUK koodi?</span></a>";
			error = ( isBlockedPin ) ? "PUK on blokeeritud, Uue PUK koodi saamiseks, külasta klienditeeninduspunkti, kust saad koodiümbriku uute koodidega." : "";
			break;
		default:
			break;
	}
    
	update(
			name,
			txt,
			changeBtn,
			forgotPinText,
			detailsText,
			error );

}

void VerifyCert::update( const QString &name, const QString &validUntil, const QString &change, const QString &forgotPinText, const QString &detailsText, const QString &error )
{
	if( !isValidCert && pinType != QSmartCardData::PukType )
	{
		this->setStyleSheet( "opacity: 0.25; background-color: #F9EBEB;"  + borders );
		ui->verticalSpacerAboveBtn->changeSize( 20, 8 );
		ui->verticalSpacerBelowBtn->changeSize( 20, 6 );
		ui->changePIN->setStyleSheet( 
					"QPushButton { border-radius: 2px; border: none; color: #ffffff; background-color: #006EB5;}" 
					"QPushButton:pressed { background-color: #41B6E6;}" 
					"QPushButton:hover:!pressed { background-color: #008DCF;}" 
					"QPushButton:disabled { background-color: #BEDBED;};" 
					); 
		ui->error->setStyleSheet(
					"padding: 6px 6px 6px 6px;"
					"line-height: 14px;"
					"box-sizing: border-box;"
					"border: 1px solid #c53e3e;"
					"border-radius: 2px;"
					"background-color: #e09797;"
					"font-family: Arial;"
					"color: #5c1c1c;"
					);
		ui->name->setTextFormat( Qt::RichText );
		ui->name->setText(name + " <img src=\":/images/icon_alert_red.svg\" height=\"12\" width=\"13\">");
	}
	else if( isBlockedPin )
	{
		this->setStyleSheet( "opacity: 0.25; background-color: #fbecd0;"  + borders );
		ui->verticalSpacerAboveBtn->changeSize( 20, 8 );
		ui->verticalSpacerBelowBtn->changeSize( 20, 6 );
		ui->changePIN->setStyleSheet( 
					"QPushButton { border-radius: 2px; border: none; color: #ffffff; background-color: #006EB5;}" 
					"QPushButton:pressed { background-color: #41B6E6;}" 
					"QPushButton:hover:!pressed { background-color: #008DCF;}" 
					"QPushButton:disabled { background-color: #BEDBED;};" 
					); 
		ui->error->setStyleSheet(
					"padding: 6px 6px 6px 6px;"
					"line-height: 14px;"
					"box-sizing: border-box;"
					"border: 1px solid #e89c30;"
					"border-radius: 2px;"
					"background-color: #F8DDA7;"
					"font-family: Arial;"
					);
		ui->name->setTextFormat(Qt::RichText);
		ui->name->setText(name + " <img src=\":/images/icon_alert_orange.svg\" height=\"12\" width=\"13\">");
	}
	else 
	{
		this->setStyleSheet( "background-color: #ffffff;" + borders );
		ui->verticalSpacerAboveBtn->changeSize( 20, 32 );
		ui->verticalSpacerBelowBtn->changeSize( 20, 38 );
		changePinStyle( "#FFFFFF" );
		ui->name->setTextFormat( Qt::RichText );
		ui->name->setText( name );
	}
	if( smartCard->data().retryCount( QSmartCardData::PukType ) == 0 )  // if PUK is blocked then disable 'change PIN/PUK' buttons.
		ui->changePIN->hide();

	ui->validUntil->setText( validUntil );
	if( pinType != QSmartCardData::PukType )
		ui->error->setVisible( isBlockedPin || !isValidCert );
	else
		ui->error->setVisible( isBlockedPin );
	ui->error->setText( error );
	ui->changePIN->setText( change ); 

	if( forgotPinText.isEmpty() )
	{
		ui->forgotPinLink->setVisible( false );
	}
	else
	{
		ui->forgotPinLink->setVisible( true );
		ui->forgotPinLink->setText( forgotPinText );
		ui->forgotPinLink->setOpenExternalLinks( true );
	}

	if( detailsText.isEmpty() )
	{
		ui->details->setVisible( false );
	}
	else
	{
		ui->details->setText( detailsText );
		ui->details->setVisible( true );
		ui->details->setOpenExternalLinks( true );
	}
}

void VerifyCert::enterEvent( QEvent * event )
{
	if( !isValidCert && pinType != QSmartCardData::PukType )
		this->setStyleSheet( "background-color: #f3d8d8;" + borders );
	else if( isBlockedPin )
		this->setStyleSheet( "background-color: #f9e2b9;" + borders );
	else
    {
		this->setStyleSheet( "background-color: #f7f7f7;" + borders );
		changePinStyle( "#f7f7f7" );  
	}  
}

void VerifyCert::leaveEvent( QEvent * event )
{
	if( !isValidCert && pinType != QSmartCardData::PukType )
		this->setStyleSheet( "background-color: #F9EBEB;" + borders );
	else if( isBlockedPin )
		this->setStyleSheet( "background-color: #fbecd0;" + borders );
	else
    {
		this->setStyleSheet( "background-color: white;" + borders );
		changePinStyle( "white" );  
	}  
}

void VerifyCert::changePinStyle( const QString &background )  
{  
 	ui->changePIN->setStyleSheet(  
 		QString("QPushButton { border-radius: 2px; border: 1px solid #006EB5; color: #006EB5; background-color: %1;}"  
 		"QPushButton:pressed { border: 1px solid #41B6E6; color: #41B6E6;}"  
 		"QPushButton:hover:!pressed { border: 1px solid #008DCF; color: #008DCF;}"  
 		"QPushButton:disabled { border: 1px solid #BEDBED; color: #BEDBED;};").arg( background )  
 		);  
}  

void VerifyCert::processClickedBtn()
{
	switch( pinType )
	{
		case QSmartCardData::Pin1Type:
        
			if( !isValidCert )
				QMessageBox::warning( this, windowTitle(), "... PIN1 Uuenda sertifikaate" );
			else if( isBlockedPin )
				( (QSmartCard *)smartCard )->pinUnblock( PinDialog::Pin1Type );
			else
				QMessageBox::warning( this, windowTitle(), "... change PIN1 Clicked" );
			break;
		case QSmartCardData::Pin2Type:
			if( !isValidCert )
				QMessageBox::warning( this, windowTitle(), "... PIN2 Uuenda sertifikaate" );
			else if( isBlockedPin )
				( (QSmartCard *)smartCard )->pinUnblock( PinDialog::Pin2Type );
			else
				QMessageBox::warning( this, windowTitle(), "... change PIN2 Clicked" );
			break;
		case QSmartCardData::PukType:
			QMessageBox::warning( this, windowTitle(), "change PUK Clicked" );
			break;
		default:
			break;
	}
}
