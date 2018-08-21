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
#include "QCardInfo.h"
#include "QCardLock.h"
#include "QSigner.h"
#include "XmlReader.h"
#ifdef Q_OS_WIN
#include "CertStore.h"
#endif
#include "dialogs/Updater.h"
#include "effects/FadeInNotification.h"
#include "util/CertUtil.h"
#include "widgets/WarningItem.h"

#include <common/Configuration.h>
#include <common/Settings.h>
#include <common/SslCertificate.h>
#include <common/TokenData.h>

#include <QtCore/QJsonObject>
#include <QDateTime>
#include <QMessageBox>
#include <QtNetwork/QSslConfiguration>
#include <QtNetwork/QSslKey>

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

void MainWindow::changePukClicked()
{
	pinPukChange(QSmartCardData::PukType);
}

bool MainWindow::checkExpiration()
{
	int expiresIn = 106;
	for(const SslCertificate &cert: {qApp->signer()->tokenauth().cert(), qApp->signer()->tokensign().cert()})
	{
		if(!cert.isNull())
		{
			expiresIn = std::min<int>(expiresIn,
				QDateTime::currentDateTime().daysTo(cert.expiryDate().toLocalTime()));
		}
	}

	if(expiresIn <= 0)
		showWarning(WarningText(WarningType::CertExpiredWarning));
	else if(expiresIn <= 105)
		showWarning(WarningText(WarningType::CertExpiryWarning));

	return (expiresIn <= 105);
}

void MainWindow::pinUnblock( QSmartCardData::PinType type, bool isForgotPin )
{
	QString text = tr("%1 has been changed and the certificate has been unblocked!")
			.arg( QSmartCardData::typeString( type ) );

	if( validateCardError( type, 1025,
		qApp->smartcard()->pinUnblock( type, isForgotPin, this ) ) )
	{
		if( isForgotPin )
			text = tr("%1 changed!").arg( QSmartCardData::typeString( type ) );
		showNotification( text, true );
		ui->accordion->updateInfo( qApp->smartcard() );
		updateCardWarnings();

		QString card = qApp->smartcard()->data().card();

		if (type == QSmartCardData::Pin1Type)
		{
			clearWarning(WarningType::UnblockPin1Warning);
			emit ui->cryptoContainerPage->cardChanged(card);
		}
		if (type == QSmartCardData::Pin2Type)
		{
			clearWarning(WarningType::UnblockPin2Warning);
			emit ui->signContainerPage->cardChanged(card);
		}
	}
}

void MainWindow::pinPukChange( QSmartCardData::PinType type )
{
	if( validateCardError( type, 1024,
		qApp->smartcard()->pinChange( type, this ) ) )
	{
		showNotification( tr("%1 changed!")
			.arg( QSmartCardData::typeString( type ) ), true );
		ui->accordion->updateInfo( qApp->smartcard() );
	}
}

void MainWindow::certDetailsClicked( const QString &link )
{
	bool pin1 = link == "PIN1";
	CertUtil::showCertificate(pin1 ? qApp->signer()->tokenauth().cert() : qApp->signer()->tokensign().cert(), this,
		pin1 ? "-auth" : "-sign");
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
	SSLConnect ssl(qApp->signer()->tokenauth().cert(), qApp->signer()->key());
	QByteArray buffer = ssl.getUrl( type, param );
	qApp->signer()->logout();
	if( !ssl.errorString().isEmpty() )
	{
		switch( type )
		{
		case SSLConnect::ActivateEmails: showWarning(WarningText(WarningType::EmailActivationWarning, ssl.errorString())); break;
		case SSLConnect::EmailInfo: showWarning(WarningText(WarningType::EmailLoadingWarning, ssl.errorString())); break;
		case SSLConnect::PictureInfo: showWarning(WarningText(WarningType::PictureLoadingWarning, ssl.errorString())); break;
		default: showWarning(WarningText(WarningType::SSLLoadingWarning, ssl.errorString())); break;
		}
		return QByteArray();
	}
	return buffer;
}

bool MainWindow::validateCardError( QSmartCardData::PinType type, int flags, QSmartCard::ErrorType err )
{
	QSmartCardData::PinType t = flags == 1025 ? QSmartCardData::PukType : type;
	QSmartCardData td = qApp->smartcard()->data();
	switch( err )
	{
	case QSmartCard::NoError: return true;
	case QSmartCard::CancelError:
#ifdef Q_OS_WIN
		if( !td.isNull() && td.isPinpad() )
		if( td.authCert().subjectInfo( "C" ) == "EE" )	// only for Estonian ID card
		{
			showNotification( tr("%1 timeout").arg( QSmartCardData::typeString( type ) ) );
		}
#endif
		break;
	case QSmartCard::BlockedError:
		showNotification( tr("%1 blocked").arg( QSmartCardData::typeString( t ) ) );
		showPinBlockedWarning(td);
		pageSelected( ui->myEid );
		ui->accordion->updateInfo( qApp->smartcard() );
		ui->myEid->warningIcon(
				qApp->smartcard()->data().retryCount( QSmartCardData::Pin1Type ) == 0 || 
				qApp->smartcard()->data().retryCount( QSmartCardData::Pin2Type ) == 0 || 
				qApp->smartcard()->data().retryCount( QSmartCardData::PukType ) == 0 );
		if(qApp->smartcard()->data().retryCount( QSmartCardData::Pin1Type ) == 0)
			clearWarning(WarningType::UpdateCertWarning);
		break;
	case QSmartCard::DifferentError:
		showNotification( tr("New %1 codes doesn't match").arg( QSmartCardData::typeString( type ) ) ); break;
	case QSmartCard::LenghtError:
		switch( type )
		{
		case QSmartCardData::Pin1Type: showNotification( tr("PIN1 length has to be between 4 and 12") ); break;
		case QSmartCardData::Pin2Type: showNotification( tr("PIN2 length has to be between 5 and 12") ); break;
		case QSmartCardData::PukType: showNotification( tr("PUK length has to be between 8 and 12") ); break;
		}
		break;
	case QSmartCard::OldNewPinSameError:
		showNotification( tr("Old and new %1 has to be different!").arg( QSmartCardData::typeString( type ) ) );
		break;
	case QSmartCard::ValidateError:
		showNotification( tr("Wrong %1 code. You can try %n more time(s).", "",
			qApp->smartcard()->data().retryCount( t ) ).arg( QSmartCardData::typeString( t ) ) );

		break;
	default:
		switch( flags )
		{
		case SSLConnect::ActivateEmails: showNotification( tr("Failed activating email forwards.") ); break;
		case SSLConnect::EmailInfo: showNotification( tr("Failed loading email settings.") ); break;
		case SSLConnect::PictureInfo: showNotification( tr("Loading picture failed.") ); break;
		default:
			showNotification( tr( "Changing %1 failed" ).arg( QSmartCardData::typeString( type ) ) ); break;
		}
		break;
	}
	return false;
}

void MainWindow::showNotification( const QString &msg, bool isSuccess )
{
	QString textColor = isSuccess ? "#ffffff" : "#353739";
	QString bkColor = isSuccess ? "#8CC368" : "#F8DDA7";
	int displayTime = isSuccess ? 2000 : 6000;

	FadeInNotification* notification = new FadeInNotification( this, textColor, bkColor, 110 );
	notification->start( msg, 750, displayTime, 600 );
}

void MainWindow::updateCertificate()
{
#ifdef Q_OS_WIN
	// remove certificates (having %ESTEID% text) from browsing history of Internet Explorer and/or Google Chrome, and do it for all users.
	CertStore s;
	for (const SslCertificate &c : s.list())
	{
		if (c.subjectInfo("O").contains("ESTEID"))
			s.remove(c);
	}
#endif
	{
		QCardLocker locker;
		Updater(qApp->smartcard()->data().reader(), this).execute();
	}
	qApp->smartcard()->reload();
}

bool MainWindow::isUpdateCertificateNeeded()
{
	QSmartCardData t = qApp->smartcard()->data();

	return
		Settings(qApp->applicationName()).value("updateButton", false).toBool() ||
		Settings().value("testUpdater", false).toBool() ||							// TODO for testing. Remove it later !!!!!!
		(
			t.version() >= QSmartCardData::VER_3_5 &&
			t.retryCount( QSmartCardData::Pin1Type ) > 0 &&
			t.isValid() &&
			Configuration::instance().object().contains("EIDUPDATER-URL-TOECC") && (
				t.authCert().publicKey().algorithm() == QSsl::Rsa ||
				t.signCert().publicKey().algorithm() == QSsl::Rsa ||
				t.version() & QSmartCardData::VER_HASUPDATER ||
				t.version() == QSmartCardData::VER_USABLEUPDATER
			)
		);
}

void MainWindow::removeOldCert()
{
#ifdef Q_OS_WIN
	// remove certificates (having %ESTEID% text) from browsing history of Internet Explorer and/or Google Chrome, and do it for all users.
	QSmartCardData data = qApp->smartcard()->data();
	CertStore s;
	for( const SslCertificate &c: s.list())
	{
		if( c.subjectInfo( "O" ).contains("ESTEID") )
			s.remove( c );
	}
	QString personalCode = data.signCert().subjectInfo("serialNumber");

	if( !personalCode.isEmpty() ) {
		s.add( data.authCert(), data.card() );
		s.add( data.signCert(), data.card() );
	}
	qApp->showWarning( tr("Redundant certificates have been successfully removed.") );
#endif
}

void MainWindow::updateCardWarnings()
{
	bool showWarning = false;
	if(isUpdateCertificateNeeded())
	{
		showWarning = true;
		showUpdateCertWarning();
	}
	if(checkExpiration())
		showWarning = true;

	if(!showWarning && !qApp->smartcard()->data().isNull())
		showWarning = qApp->smartcard()->data().retryCount( QSmartCardData::Pin1Type ) == 0 || 
			qApp->smartcard()->data().retryCount( QSmartCardData::Pin2Type ) == 0 || 
			qApp->smartcard()->data().retryCount( QSmartCardData::PukType ) == 0;
	ui->myEid->warningIcon(showWarning);
}
