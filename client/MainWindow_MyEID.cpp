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

#include "MainWindow.h"
#include "ui_MainWindow.h"
#include "Application.h"
#include "QSigner.h"
#include "XmlReader.h"

#include "effects/FadeInNotification.h"
#include <common/SslCertificate.h>

#include <QtNetwork/QSslConfiguration>

using namespace ria::qdigidoc4;

void MainWindow::changePin1Clicked( bool isForgotPin, bool isBlockedPin )
{
	QSmartCardData::PinType type = QSmartCardData::Pin1Type;

	if( isForgotPin )
		pinUnblock( type, isForgotPin );
	else if( isBlockedPin )
		pinUnblock( type );
	else
		pinPukChange( type );
}

void MainWindow::changePin2Clicked( bool isForgotPin, bool isBlockedPin )
{
	QSmartCardData::PinType type = QSmartCardData::Pin2Type;
    
	if( isForgotPin )
		pinUnblock( type, isForgotPin );
	else if( isBlockedPin )
		pinUnblock( type );
	else
		pinPukChange( type );
}

void MainWindow::changePukClicked( bool isForgotPuk )
{
	if( isForgotPuk )
		QMessageBox::warning( this, windowTitle(), "... PUK forgotten" );
	else
		pinPukChange( QSmartCardData::PukType );
}

void MainWindow::pinUnblock( QSmartCardData::PinType type, bool isForgotPin )
{
	QString text = tr("%1 kood on muudetud ja sertifikaadi blokeering tühistatud!")
			.arg( QSmartCardData::typeString( type ) );

	if( validateCardError( type, 1025,
		( (QSmartCard *)smartcard )->pinUnblock( type, isForgotPin ) ) )
	{
		if( isForgotPin )
			text = tr("%1 kood muudetud!").arg( QSmartCardData::typeString( type ) );
		showNotification( text, true );
        ui->accordion->updateInfo( smartcard );
		ui->myEid->pinIsBlockedIcon( 
				smartcard->data().retryCount( QSmartCardData::Pin1Type ) == 0 || 
				smartcard->data().retryCount( QSmartCardData::Pin2Type ) == 0 || 
				smartcard->data().retryCount( QSmartCardData::PukType ) == 0 );
	}
}

void MainWindow::pinPukChange( QSmartCardData::PinType type )
{
	if( validateCardError( type, 1024,
		( (QSmartCard *)smartcard )->pinChange( type ) ) )
	{
		showNotification( tr("%1 kood muudetud!")
			.arg( QSmartCardData::typeString( type ) ), true );
        ui->accordion->updateInfo( smartcard );
	}
}

void MainWindow::certDetailsClicked( const QString &link )
{
	QMessageBox::warning( this, windowTitle(), "... Certificate details" );
}

void MainWindow::getEmailStatus ()
{
	QByteArray buffer = sendRequest( SSLConnect::EmailInfo );

	if( buffer.isEmpty() )
		return;

	XmlReader xml( buffer );
	QString error;
	QMultiHash<QString,QPair<QString,bool> > emails = xml.readEmailStatus( error );
	quint8 code = error.toUInt();
	if( emails.isEmpty() || code > 0 )
	{
		code = code ? code : 20;
		if( code == 20 )
			ui->accordion->updateOtherData( true, XmlReader::emailErr( code ), code );
        else
			ui->accordion->updateOtherData( false, XmlReader::emailErr( code ), code );
	}
	else
	{
		QStringList text;
		for( Emails::const_iterator i = emails.constBegin(); i != emails.constEnd(); ++i )
		{
			text << QString( "%1 - %2 (%3)" )
				.arg( i.key() )
				.arg( i.value().first )
				.arg( i.value().second ? tr("active") : tr("not active") );
		}
		ui->accordion->updateOtherData( false, text.join("<br />") );
	}
}

void MainWindow::activateEmail ()
{
	QString eMail = ui->accordion->getEmail();
	if( eMail.isEmpty() )
	{
		showNotification( tr("E-mail address missing or invalid!") );
        ui->accordion->setFocusToEmail();
		return;
	}
	QByteArray buffer = sendRequest( SSLConnect::ActivateEmails, eMail );
	if( buffer.isEmpty() )
		return;
	XmlReader xml( buffer );
	QString error;
	xml.readEmailStatus( error );
	ui->accordion->updateOtherData( false, XmlReader::emailErr( error.toUInt() ) );
	return;
}

QByteArray MainWindow::sendRequest( SSLConnect::RequestType type, const QString &param )
{
/*
	switch( type )
	{
	case SSLConnect::ActivateEmails: showLoading(  tr("Activating email settings") ); break;
	case SSLConnect::EmailInfo: showLoading( tr("Loading email settings") ); break;
	case SSLConnect::MobileInfo: showLoading( tr("Requesting Mobiil-ID status") ); break;
	case SSLConnect::PictureInfo: showLoading( tr("Loading picture") ); break;
	default: showLoading( tr( "Loading data" ) ); break;
	}
*/
	if( !validateCardError( QSmartCardData::Pin1Type, type, smartcard->login( QSmartCardData::Pin1Type ) ) )
		return QByteArray();

	SSLConnect ssl;
	ssl.setToken( smartcard->data().authCert(), smartcard->key() );
	QByteArray buffer = ssl.getUrl( type, param );
	smartcard->logout();
//	updateData();
	if( !ssl.errorString().isEmpty() )
	{
		switch( type )
		{
		case SSLConnect::ActivateEmails: showWarning( tr("Failed activating email forwards."), ssl.errorString() ); break;
		case SSLConnect::EmailInfo: showWarning( tr("Failed loading email settings."), ssl.errorString() ); break;
		case SSLConnect::MobileInfo: showWarning( tr("Failed loading Mobiil-ID settings."), ssl.errorString() ); break;
		case SSLConnect::PictureInfo: showWarning( tr("Loading picture failed."), ssl.errorString() ); break;
		default: showWarning( tr("Failed to load data"), ssl.errorString() ); break;
		}
		return QByteArray();
	}
	return buffer;
}

bool MainWindow::validateCardError( QSmartCardData::PinType type, int flags, QSmartCard::ErrorType err )
{
	// updateData();
	QSmartCardData::PinType t = flags == 1025 ? QSmartCardData::PukType : type;
	QSmartCardData td = smartcard->data();
	switch( err )
	{
	case QSmartCard::NoError: return true;
	case QSmartCard::CancelError:
#ifdef Q_OS_WIN
		if( !td.isNull() && td.isPinpad() )
		if( td.authCert().subjectInfo( "C" ) == "EE" )	// only for Estonian ID card
		{
			switch ( type )
			{
			case QSmartCardData::Pin1Type: showNotification( "PIN1 timeout" ); break;
			case QSmartCardData::Pin2Type: showNotification( "PIN2 timeout" ); break;
			case QSmartCardData::PukType: showNotification( "PUK timeout" ); break;
			}
		}
#endif
		break;
	case QSmartCard::BlockedError:
		showNotification( QString("%1 blocked").arg( QSmartCardData::typeString( t ) ) );
		pageSelected( ui->myEid );
		ui->accordion->updateInfo( smartcard );
		ui->myEid->pinIsBlockedIcon( 
				smartcard->data().retryCount( QSmartCardData::Pin1Type ) == 0 || 
				smartcard->data().retryCount( QSmartCardData::Pin2Type ) == 0 || 
				smartcard->data().retryCount( QSmartCardData::PukType ) == 0 );
		break;
	case QSmartCard::DifferentError:
		showNotification( QString("New %1 codes doesn't match").arg( QSmartCardData::typeString( type ) ) ); break;
	case QSmartCard::LenghtError:
		switch( type )
		{
		case QSmartCardData::Pin1Type: showNotification( "PIN1 length has to be between 4 and 12" ); break;
		case QSmartCardData::Pin2Type: showNotification( "PIN2 length has to be between 5 and 12" ); break;
		case QSmartCardData::PukType: showNotification( "PUK length has to be between 8 and 12" ); break;
		}
		break;
	case QSmartCard::OldNewPinSameError:
		showNotification( QString("Old and new %1 has to be different!").arg( QSmartCardData::typeString( type ) ) );
		break;
	case QSmartCard::ValidateError:
		showNotification( QString("Wrong %1 code.").arg( QSmartCardData::typeString( t ) ) + QString("You can try %1 more time(s).").arg( smartcard->data().retryCount( t ) ));
		break;
	default:
		switch( flags )
		{
		case SSLConnect::ActivateEmails: showNotification( "Failed activating email forwards." ); break;
		case SSLConnect::EmailInfo: showNotification( "Failed loading email settings." ); break;
		case SSLConnect::MobileInfo: showNotification( "Failed loading Mobiil-ID settings." ); break;
		case SSLConnect::PictureInfo: showNotification( "Loading picture failed." ); break;
		default:
			showNotification( QString( "Changing %1 failed" ).arg( QSmartCardData::typeString( type ) ) ); break;
		}
		break;
	}
	return false;
}

void MainWindow::showNotification( const QString &msg, bool isSuccess )
{
    QString textColor = isSuccess ? "#ffffff" : "#353739";
    QString bkColor = isSuccess ? "#8CC368" : "#F8DDA7";
    int displayTime = isSuccess ? 2000 : 3000;
    
	FadeInNotification* notification = new FadeInNotification( this, textColor, bkColor, 110 );
	notification->start( msg, 750, 2500, 600 );
}

void MainWindow::getOtherEID ()
{
	getMobileIdStatus ();
	getDigiIdStatus ();
}

void MainWindow::getMobileIdStatus ()
{
	QByteArray buffer = sendRequest( SSLConnect::MobileInfo );
	if( buffer.isEmpty() )
		return;

	XmlReader xml( buffer );
	int error = 0;
	QVariant mobileData = QVariant::fromValue( xml.readMobileStatus( error ) );
	if( error )
	{
		showWarning( XmlReader::mobileErr( error ), QString() );
		return;
	}
	ui->accordion->setProperty( "MOBILE_ID_STATUS", mobileData );
	ui->accordion->updateMobileIdInfo();
}

void MainWindow::getDigiIdStatus ()
{
	// TODO
	ui->accordion->setProperty( "DIGI_ID_STATUS", QVariant() );
	ui->accordion->updateDigiIdInfo();
}
