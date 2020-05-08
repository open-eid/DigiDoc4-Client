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
#include "SslCertificate.h"
#include "effects/FadeInNotification.h"
#include "widgets/WarningItem.h"
#include "widgets/WarningList.h"

#include <QDateTime>
#include <QtNetwork/QSslKey>

using namespace ria::qdigidoc4;

void MainWindow::changePin1Clicked( bool isForgotPin, bool isBlockedPin )
{
	QSmartCardData::PinType type = QSmartCardData::Pin1Type;
	if(isForgotPin || isBlockedPin)
		pinUnblock(type, isForgotPin);
	else
		pinPukChange( type );
}

void MainWindow::changePin2Clicked( bool isForgotPin, bool isBlockedPin )
{
	QSmartCardData::PinType type = QSmartCardData::Pin2Type;
	if(isForgotPin || isBlockedPin)
		pinUnblock(type, isForgotPin);
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

	if(validateCardError(type, QSmartCardData::PukType,
		qApp->signer()->smartcard()->pinUnblock(type, isForgotPin, this)))
	{
		if( isForgotPin )
			text = tr("%1 changed!").arg( QSmartCardData::typeString( type ) );
		showNotification( text, true );
		QSmartCardData data = qApp->signer()->smartcard()->data();
		updateCardWarnings(data);
		if (type == QSmartCardData::Pin1Type)
		{
			warnings->closeWarning(WarningType::UnblockPin1Warning);
			ui->cryptoContainerPage->cardChanged(data.authCert(),
				data.retryCount(QSmartCardData::Pin1Type) == 0);
		}
		if (type == QSmartCardData::Pin2Type)
		{
			warnings->closeWarning(WarningType::UnblockPin2Warning);
			ui->signContainerPage->cardChanged(data.signCert(),
				data.retryCount(QSmartCardData::Pin2Type) == 0);
		}
	}
}

void MainWindow::pinPukChange( QSmartCardData::PinType type )
{
	if(validateCardError(type, type,
		qApp->signer()->smartcard()->pinChange(type, this)))
	{
		showNotification( tr("%1 changed!")
			.arg( QSmartCardData::typeString( type ) ), true );
	}
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
	if(ui->accordion->updateOtherData(buffer))
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
		return {};
	}
	return buffer;
}

bool MainWindow::validateCardError(QSmartCardData::PinType type, QSmartCardData::PinType t, QSmartCard::ErrorType err)
{
	QSmartCardData data = qApp->signer()->smartcard()->data();
	ui->accordion->updateInfo(data);
	switch( err )
	{
	case QSmartCard::NoError: return true;
	case QSmartCard::CancelError:
#ifdef Q_OS_WIN
		if(!data.isNull() && data.isPinpad())
		if(data.authCert().subjectInfo(QSslCertificate::CountryName) == QStringLiteral("EE")) // only for Estonian ID card
		{
			showNotification( tr("%1 timeout").arg( QSmartCardData::typeString( type ) ) );
		}
#endif
		break;
	case QSmartCard::BlockedError:
		showNotification( tr("%1 blocked").arg( QSmartCardData::typeString( t ) ) );
		showPinBlockedWarning(data);
		pageSelected( ui->myEid );
		ui->myEid->warningIcon(
			data.retryCount(QSmartCardData::Pin1Type) == 0 ||
			data.retryCount(QSmartCardData::Pin2Type) == 0 ||
			data.retryCount(QSmartCardData::PukType) == 0 );
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
			data.retryCount(t)).arg( QSmartCardData::typeString(t)));
		break;
	default:
		showNotification(tr("Changing %1 failed").arg(QSmartCardData::typeString(type)));
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

void MainWindow::showPinBlockedWarning(const QSmartCardData& t)
{
	bool isBlockedPuk = t.retryCount(QSmartCardData::PukType) == 0;
	if(!isBlockedPuk && t.retryCount(QSmartCardData::Pin2Type) == 0)
		warnings->showWarning(WarningText(WarningType::UnblockPin2Warning));
	if(!isBlockedPuk && t.retryCount(QSmartCardData::Pin1Type) == 0)
		warnings->showWarning(WarningText(WarningType::UnblockPin1Warning));
	ui->signContainerPage->cardChanged(t.signCert(),
		t.retryCount(QSmartCardData::Pin2Type) == 0);
	ui->cryptoContainerPage->cardChanged(t.authCert(),
		t.retryCount(QSmartCardData::Pin1Type) == 0);
}

void MainWindow::updateCardWarnings(const QSmartCardData &data)
{
	const qint64 DAY = 24 * 60 * 60;
	qint64 expiresIn = 106 * DAY;
	for(const QSslCertificate &cert: {data.authCert(), data.signCert()})
	{
		if(!cert.isNull())
		{
			expiresIn = std::min<qint64>(expiresIn,
				QDateTime::currentDateTime().secsTo(cert.expiryDate().toLocalTime()));
		}
	}

	if(!data.isNull())
		ui->myEid->warningIcon(
			data.retryCount(QSmartCardData::Pin1Type) == 0 ||
			data.retryCount(QSmartCardData::Pin2Type) == 0 ||
			data.retryCount(QSmartCardData::PukType) == 0);
	if(expiresIn <= 0)
	{
		ui->myEid->invalidIcon(true);
		warnings->showWarning(WarningText(WarningType::CertExpiredWarning));
	}
	else if(data.version() >= QSmartCardData::VER_3_5 && data.version() < QSmartCardData::VER_IDEMIA &&
		data.authCert().publicKey().algorithm() == QSsl::Rsa)
	{
		ui->myEid->invalidIcon(true);
		warnings->showWarning(WarningText(WarningType::CertRevokedWarning));
	}
	else if(expiresIn <= 105 * DAY)
	{
		ui->myEid->warningIcon(true);
		warnings->showWarning(WarningText(WarningType::CertExpiryWarning));
	}
}

void MainWindow::updateMyEID(const TokenData &t)
{
	warnings->clearMyEIDWarnings();
	SslCertificate cert(t.cert());
	int type = cert.type();
	ui->infoStack->setHidden(t.isNull() || type == SslCertificate::UnknownType || type == SslCertificate::EidType);
	ui->accordion->setHidden(t.isNull() || type == SslCertificate::UnknownType || type == SslCertificate::EidType);
	ui->noReaderInfo->setVisible(t.isNull() || type == SslCertificate::UnknownType || type == SslCertificate::EidType);

	if(!t.isNull())
	{
		ui->noReaderInfoText->setProperty("currenttext", "The card in the card reader is not an Estonian ID-card");
		ui->noReaderInfoText->setText(tr("The card in the card reader is not an Estonian ID-card"));
		if(ui->cardInfo->token().card() != t.card())
			ui->accordion->clear();
		if(type & SslCertificate::TempelType)
		{
			ui->infoStack->update(cert);
			ui->accordion->updateInfo(cert);
		}
	}
	else
	{
		ui->infoStack->setProperty("PICTURE", QVariant());
		ui->infoStack->clearData();
		ui->accordion->clear();
		ui->myEid->invalidIcon(false);
		ui->myEid->warningIcon(false);
		ui->noReaderInfoText->setProperty("currenttext", "Connect the card reader to your computer and insert your ID card into the reader");
		ui->noReaderInfoText->setText(tr("Connect the card reader to your computer and insert your ID card into the reader"));
	}
}

void MainWindow::updateMyEid(const QSmartCardData &t)
{
	if(!t.card().isEmpty() && (!t.authCert().isNull() || !t.signCert().isNull()))
	{
		ui->infoStack->update(t);
		ui->accordion->updateInfo(t);
		updateCardWarnings(t);
		showPinBlockedWarning(t);
	}
}
