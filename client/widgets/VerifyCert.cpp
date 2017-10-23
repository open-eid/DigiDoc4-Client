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
	connect( ui->forgotPinLink, &QLabel::linkActivated, this, &VerifyCert::processForgotPinLink );
	connect( ui->details, &QLabel::linkActivated, this, &VerifyCert::processCertDetails );

	ui->name->setFont( Styles::font( Styles::Regular, 18, QFont::Bold  ) );
	ui->validUntil->setFont( Styles::font( Styles::Regular, 14 ) );
	ui->error->setFont( Styles::font( Styles::Regular, 12, QFont::DemiBold ) );
	QFont regular12 = Styles::font( Styles::Regular, 12 );
	ui->forgotPinLink->setFont( regular12 );
	ui->details->setFont( regular12 );
	ui->changePIN->setFont( Styles::font( Styles::Condensed, 14 ) );
	borders = " border: solid #DFE5E9; border-width: 1px 0px 0px 0px; ";
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

void VerifyCert::update( QSmartCardData::PinType type, const QSmartCard *pSmartCard )
{
	QString name;
	QString validUntil;
	QString changeBtn;
	QString forgotPinText;
	QString detailsText;
	QString error;
	QString txt;
	QTextStream cert( &txt );

	this->pinType = type;
	QSmartCardData t = pSmartCard->data();
	
	SslCertificate c = ( type == QSmartCardData::Pin1Type ) ? t.authCert() : t.signCert();
	this->isValidCert = c.isValid();
	this->isBlockedPin = (t.retryCount( type ) == 0) ? true : false;

	if( !isValidCert )
	{
		cert << tr("Sertifikaat on aegunud!");
	}
	else
	{
		cert << "Sertifikaat <span style='color: #37a447'>kehtib</span> kuni "
			<< DateTime(c.expiryDate().toLocalTime()).formatDate("dd. MMMM yyyy");

		int leftDays = std::max<int>( 0, QDateTime::currentDateTime().daysTo( c.expiryDate().toLocalTime() ) );
		if( leftDays <= 105 && t.retryCount( type ) != 0 )
			cert << "<br /><span style='color: red'>" << tr("Sertifikaat aegub %1 päeva pärast").arg( leftDays ) << "</span>";
	}
	switch( type )
	{
		case QSmartCardData::Pin1Type:
			name = "Isikutuvastamise sertifikaat";
			changeBtn = ( isBlockedPin ) ? "TÜHISTA BLOKEERING" : "MUUDA PIN1";
			forgotPinText = "<a href='#pin1-forgotten'><span style='color:#75787B;'>Unustasid PIN1 koodi?</span></a>";
			detailsText = "<a href='#pin1-cert'><span style='color:#75787B;'>Vaata sertifikaadi detaile</span></a>";
			error = ( !isValidCert ) ? "PIN1 ei saa kasutada, kuna sertifikaat on aegunud. Uuenda sertifikaat, et PIN1 taas kasutada." :
					( isBlockedPin ) ? "PIN1 on blokeeritud, kuna PIN1 koodi on sisestatud 3 korda valesti. Tühista blokeering, et PIN1 taas kasutada." :
					"";
			break;
		case QSmartCardData::Pin2Type:
			name = "Allkirjastamise sertifikaat";
			changeBtn = ( isBlockedPin ) ? "TÜHISTA BLOKEERING" : "MUUDA PIN2";
			forgotPinText = "<a href='#pin2-forgotten'><span style='color:#75787B;'>Unustasid PIN2 koodi?</span></a>";
			detailsText = "<a href='#pin2-cert'><span style='color:#75787B;'>Vaata sertifikaadi detaile</span></a>";
			error = ( !isValidCert ) ? "PIN2 ei saa kasutada, kuna sertifikaat on aegunud. Uuenda sertifikaat, et PIN2 taas kasutada." :
					( isBlockedPin ) ? "PIN2 on blokeeritud, kuna PIN2 koodi on sisestatud 3 korda valesti. Tühista blokeering, et PIN2 taas kasutada." : 
					"";
			break;
		case QSmartCardData::PukType:
			name = "PUK kood";
			txt = "PUK kood asub Sinu koodiümbrikus";
			changeBtn = "MUUDA PUK"; 
			error = ( isBlockedPin ) ? 
				"<span>PUK kood on blokeeritud, kuna PUK koodi on sisestatud 3 korda valesti. PUK koodi ei saa ise lahti blokeerida."
				"<br><br>Kuigi PUK kood on blokeeritud, saab kõiki eID võimalusi kasutada, välja arvatud PUK koodi vajavaid."
				"<br><br>Uue PUK koodi saad vaid uue koodiümbrikuga, mida <u>taotle PPA-st</u></span>."
				: "";
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
			error,
			t.retryCount( QSmartCardData::PukType ) == 0 );
}

void VerifyCert::update(
	const QString &name,
	const QString &validUntil,
	const QString &change,
	const QString &forgotPinText,
	const QString &detailsText,
	const QString &error,
	bool isBlockedPuk)
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
		ui->name->setText(name + " <img src=\":/images/icon_alert_red.svg\" height=\"12\" width=\"13\">");
	}
	else if( isBlockedPin )
	{
		this->setStyleSheet( "opacity: 0.25; background-color: #fcf5ea;"  + borders );
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
		ui->name->setText( name + " <img src=\":/images/icon_alert_orange.svg\" height=\"12\" width=\"13\">" );
	}
	else 
	{
		this->setStyleSheet( "background-color: #ffffff;" + borders );
		// Check height: if warning shown, decrease height by 30px (15*2)
		int decrease = height() < 210 ? 15 : 0;
		ui->verticalSpacerAboveBtn->changeSize(20, 32 - decrease);
		ui->verticalSpacerBelowBtn->changeSize(20, 38 - decrease);
		changePinStyle( "#FFFFFF" );
		ui->name->setText( name + 
			( ( pinType != QSmartCardData::PukType ) ? " <img src=\":/images/icon_check.svg\" height=\"12\" width=\"13\">" : "" ) );
	}
	ui->name->setTextFormat( Qt::RichText );
	ui->changePIN->show();

	ui->forgotPinLink->setText( forgotPinText );
	ui->forgotPinLink->setVisible( !forgotPinText.isEmpty() );

	ui->details->setText( detailsText );
	ui->details->setVisible( !detailsText.isEmpty() );
	
	if( isBlockedPuk )
	{
		if( pinType != QSmartCardData::PukType )
		{
			ui->forgotPinLink->setVisible( false ); // hide 'Forgot PIN. code?' label
			if( isBlockedPin )
				ui->changePIN->setVisible( false );  // hide 'Tühista BLOKEERING' button
		}
		else
		{
			ui->widget->setMinimumHeight ( 0 );
			ui->verticalSpacerBelowBtn->changeSize( 20, 0 );
			ui->validUntil->setVisible( false );
			ui->changePIN->hide();  // hide 'change PUK' button
		}
	}
	ui->validUntil->setText( validUntil );
	if( pinType != QSmartCardData::PukType )
		ui->error->setVisible( isBlockedPin || !isValidCert );
	else
		ui->error->setVisible( isBlockedPin );
	ui->error->setText( error );
	ui->changePIN->setText( change ); 
}

void VerifyCert::enterEvent( QEvent * event )
{
	if( !isValidCert && pinType != QSmartCardData::PukType )
		this->setStyleSheet( "background-color: #f3d8d8;" + borders );
	else if( isBlockedPin )
		this->setStyleSheet( "background-color: #fbecd0;" + borders );
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
		this->setStyleSheet( "background-color: #fcf5ea;" + borders );
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
	emit changePinClicked( false, isBlockedPin );
}

void VerifyCert::processForgotPinLink( QString link )
{
	emit changePinClicked( true, false );	// Change PIN with PUK code
}

void VerifyCert::processCertDetails( QString link )
{
	emit certDetailsClicked( (pinType == QSmartCardData::Pin1Type) ? "PIN1" : "PIN2" );
}
