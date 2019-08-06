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
#include "QCardLock.h"
#include "QSigner.h"
#ifdef Q_OS_WIN
#include "CertStore.h"
#endif
#include "dialogs/CertificateDetails.h"
#include "effects/FadeInNotification.h"
#include "widgets/WarningList.h"

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
			warnings->closeWarning(WarningType::UnblockPin1Warning);
			emit ui->cryptoContainerPage->cardChanged(card);
		}
		if (type == QSmartCardData::Pin2Type)
		{
			warnings->closeWarning(WarningType::UnblockPin2Warning);
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
	bool pin1 = link == QStringLiteral("PIN1");
	CertificateDetails::showCertificate(pin1 ? qApp->signer()->tokenauth().cert() : qApp->signer()->tokensign().cert(), this,
		pin1 ? QStringLiteral("-auth") : QStringLiteral("-sign"));
}

void MainWindow::getEmailStatus()
{
	QByteArray buffer = sendRequest( SSLConnect::EmailInfo );
	if(!buffer.isEmpty())
		ui->accordion->updateOtherData(buffer);
}

void MainWindow::activateEmail()
{
	QString eMail = ui->accordion->getEmail();
	if( eMail.isEmpty() )
	{
		showNotification( tr("E-mail address missing or invalid!") );
		ui->accordion->setFocusToEmail();
		return;
	}
	QByteArray buffer = sendRequest( SSLConnect::ActivateEmails, eMail );
	if(buffer.isEmpty())
		return;
	ui->accordion->updateOtherData(buffer);
	showNotification(tr("Succeeded activating email forwards."), true);
}

QByteArray MainWindow::sendRequest( SSLConnect::RequestType type, const QString &param )
{
	QSslKey key = qApp->signer()->key();
	QString err;
	QByteArray buffer;
	if(!key.isNull())
	{
		SSLConnect ssl(qApp->signer()->tokenauth().cert(), key);
		buffer = ssl.getUrl( type, param );
		qApp->signer()->logout();
		err = ssl.errorString();
	}
	if(!err.isEmpty() || key.isNull())
	{
		switch( type )
		{
		case SSLConnect::ActivateEmails: showNotification(tr("Failed activating email forwards.")); break;
		case SSLConnect::EmailInfo: showNotification(tr("Failed loading email settings.")); break;
		case SSLConnect::PictureInfo: showNotification(tr("Loading picture failed.")); break;
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
		if(td.authCert().subjectInfo(QSslCertificate::CountryName) == QStringLiteral("EE"))	// only for Estonian ID card
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
		break;
	case QSmartCard::DifferentError:
		showNotification( tr("New %1 codes doesn't match").arg( QSmartCardData::typeString( type ) ) );
		break;
	case QSmartCard::LenghtError:
		showNotification(tr("%1 length has to be between %2 and 12").arg(QSmartCardData::typeString(type)).arg(QSmartCardData::minPinLen(type)));
		break;
	case QSmartCard::OldNewPinSameError:
		showNotification( tr("Old and new %1 has to be different!").arg( QSmartCardData::typeString( type ) ) );
		break;
	case QSmartCard::ValidateError:
		showNotification( tr("Wrong %1 code. You can try %n more time(s).", nullptr,
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
	FadeInNotification* notification = new FadeInNotification(this,
		isSuccess ? QStringLiteral("#ffffff") : QStringLiteral("#353739"),
		isSuccess ? QStringLiteral("#8CC368") : QStringLiteral("#F8DDA7"), 110);
	notification->start(msg, 750, 15000, 600);
}

void MainWindow::removeOldCert()
{
#ifdef Q_OS_WIN
	// remove certificates (having %ESTEID% text) from browsing history of Internet Explorer and/or Google Chrome, and do it for all users.
	QSmartCardData data = qApp->smartcard()->data();
	CertStore s;
	for(const QSslCertificate &c: s.list())
	{
		if(c == data.authCert() || c == data.signCert())
			continue;
		if(c.subjectInfo(QSslCertificate::Organization).join("").contains(QStringLiteral("ESTEID"), Qt::CaseInsensitive) ||
			c.issuerInfo(QSslCertificate::Organization).contains(QStringLiteral("SK ID Solutions AS"), Qt::CaseInsensitive))
			s.remove( c );
	}
	qApp->showWarning( tr("Redundant certificates have been successfully removed.") );
#endif
}

void MainWindow::updateCardWarnings()
{
	QSmartCardData t = qApp->smartcard()->data();
	qint64 expiresIn = 106;
	for(const QSslCertificate &cert: {qApp->signer()->tokenauth().cert(), qApp->signer()->tokensign().cert()})
	{
		if(!cert.isNull())
		{
			expiresIn = std::min<qint64>(expiresIn,
				QDateTime::currentDateTime().daysTo(cert.expiryDate().toLocalTime()));
		}
	}

	if(!t.isNull())
		ui->myEid->warningIcon(
			t.retryCount( QSmartCardData::Pin1Type ) == 0 ||
			t.retryCount( QSmartCardData::Pin2Type ) == 0 ||
			t.retryCount( QSmartCardData::PukType ) == 0);
	if(expiresIn <= 0)
	{
		ui->myEid->invalidIcon(true);
		warnings->showWarning(WarningText(WarningType::CertExpiredWarning));
	}
	else if(t.version() >= QSmartCardData::VER_3_5 && t.version() < QSmartCardData::VER_IDEMIA &&
		t.authCert().publicKey().algorithm() == QSsl::Rsa)
	{
		ui->myEid->invalidIcon(true);
		warnings->showWarning(WarningText(WarningType::CertRevokedWarning));
	}
	else if(expiresIn <= 105)
	{
		ui->myEid->warningIcon(true);
		warnings->showWarning(WarningText(WarningType::CertExpiryWarning));
	}
}

void MainWindow::updateMyEid()
{
	Application::restoreOverrideCursor();
	QSmartCardData t = qApp->smartcard()->data();

	if(!t.card().isEmpty() && (!t.authCert().isNull() || !t.signCert().isNull()))
	{
		ui->infoStack->update(t);
		ui->accordion->updateInfo(qApp->smartcard());
		updateCardWarnings();
		showPinBlockedWarning(t);
		if(t.version() == QSmartCardData::VER_3_4 &&
			t.authCert().isValid() && t.signCert().isValid() &&
			(!t.authCert().validateEncoding() || !t.signCert().validateEncoding()))
		{
			qApp->showWarning(tr("Your ID-card certificates cannot be renewed starting from %1.").arg(QStringLiteral("01.07.2017")) + " " +
				tr("Your document is still valid until its expiring date and it can be used to login to e-services and give digital signatures. "
					"If there are problems using Your ID-card in e-services please contact ID-card helpdesk by phone (+372) 666 8888 or visit "
					"Police and Border Guard Board service point.<br /><br /><a href=\"http://id.ee/?id=30519&read=38011\">More info</a>"));
		}
	}
}
