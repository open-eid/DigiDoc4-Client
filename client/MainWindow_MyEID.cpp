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
		FadeInNotification::success(ui->topBar, text);
		QSmartCardData data = qApp->signer()->smartcard()->data();
		updateCardWarnings(data);
		if (type == QSmartCardData::Pin1Type)
		{
			ui->warnings->closeWarning(WarningType::UnblockPin1Warning);
			ui->cryptoContainerPage->cardChanged(data.authCert(),
				data.retryCount(QSmartCardData::Pin1Type) == 0);
		}
		if (type == QSmartCardData::Pin2Type)
		{
			ui->warnings->closeWarning(WarningType::UnblockPin2Warning);
			ui->signContainerPage->cardChanged(data.signCert(),
				data.retryCount(QSmartCardData::Pin2Type) == 0);
		}
	}
}

void MainWindow::pinPukChange( QSmartCardData::PinType type )
{
	if(validateCardError(type, type, qApp->signer()->smartcard()->pinChange(type, this)))
		FadeInNotification::success(ui->topBar, tr("%1 changed!").arg(QSmartCardData::typeString(type)));
}

bool MainWindow::validateCardError(QSmartCardData::PinType type, QSmartCardData::PinType src, QSmartCard::ErrorType err)
{
	QSmartCardData data = qApp->signer()->smartcard()->data();
	ui->accordion->updateInfo(data);
	switch( err )
	{
	case QSmartCard::NoError: return true;
	case QSmartCard::CancelError: break;
	case QSmartCard::TimeoutError:
		FadeInNotification::warning(ui->topBar, tr("%1 timeout").arg(QSmartCardData::typeString(type)));
		break;
	case QSmartCard::BlockedError:
		FadeInNotification::warning(ui->topBar, tr("%1 blocked").arg(QSmartCardData::typeString(src)));
		showPinBlockedWarning(data);
		selectPage(Pages::MyEid);
		ui->myEid->warningIcon(
			data.retryCount(QSmartCardData::Pin1Type) == 0 ||
			data.retryCount(QSmartCardData::Pin2Type) == 0 ||
			data.retryCount(QSmartCardData::PukType) == 0 );
		break;
	case QSmartCard::DifferentError:
		FadeInNotification::warning(ui->topBar, tr("New %1 codes doesn't match").arg(QSmartCardData::typeString(type)));
		break;
	case QSmartCard::LenghtError:
		FadeInNotification::warning(ui->topBar, tr("%1 length has to be between %2 and 12")
			.arg(QSmartCardData::typeString(type)).arg(QSmartCardData::minPinLen(type)));
		break;
	case QSmartCard::OldNewPinSameError:
		FadeInNotification::warning(ui->topBar, tr("Old and new %1 has to be different!").arg(QSmartCardData::typeString(type)));
		break;
	case QSmartCard::ValidateError:
		FadeInNotification::warning(ui->topBar, tr("Wrong %1 code. You can try %n more time(s).", nullptr,
			data.retryCount(src)).arg(QSmartCardData::typeString(src)));
		break;
	default:
		FadeInNotification::warning(ui->topBar, tr("Changing %1 failed").arg(QSmartCardData::typeString(type)));
		break;
	}
	return false;
}

void MainWindow::showPinBlockedWarning(const QSmartCardData& t)
{
	bool isBlockedPuk = t.retryCount(QSmartCardData::PukType) == 0;
	if(!isBlockedPuk && t.retryCount(QSmartCardData::Pin2Type) == 0)
		ui->warnings->showWarning({WarningType::UnblockPin2Warning});
	if(!isBlockedPuk && t.retryCount(QSmartCardData::Pin1Type) == 0)
		ui->warnings->showWarning({WarningType::UnblockPin1Warning});
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

	ui->myEid->invalidIcon(false);
	if(!data.isNull())
		ui->myEid->warningIcon(
			data.retryCount(QSmartCardData::Pin1Type) == 0 ||
			data.retryCount(QSmartCardData::Pin2Type) == 0 ||
			data.retryCount(QSmartCardData::PukType) == 0);
	if(expiresIn <= 0)
	{
		ui->myEid->invalidIcon(true);
		ui->warnings->showWarning({WarningType::CertExpiredError});
	}
	else if(expiresIn <= 105 * DAY)
	{
		ui->myEid->warningIcon(true);
		ui->warnings->showWarning({WarningType::CertExpiryWarning});
	}
}

void MainWindow::updateMyEID(const TokenData &t)
{
	ui->warnings->clearMyEIDWarnings();
	SslCertificate cert(t.cert());
	auto type = cert.type();
	ui->infoStack->setHidden(type == SslCertificate::UnknownType || type == SslCertificate::OldEstEidType);
	ui->accordion->setHidden(type == SslCertificate::UnknownType || type == SslCertificate::OldEstEidType);
	ui->noReaderInfo->setVisible(type == SslCertificate::UnknownType || type == SslCertificate::OldEstEidType);

	auto setText = [this](const char *text) {
		ui->noReaderInfoText->setProperty("currenttext", text);
		ui->noReaderInfoText->setText(tr(text));
	};
	if(type == SslCertificate::OldEstEidType)
		setText(QT_TR_NOOP(
			"The ID-card in the card reader has expired and is no longer supported in the DigiDoc4 Client.<br />"
			"You can apply for a new ID-card from the Estonian Police and Border Guard Board.<br />"
			"<a href=\"https://www.politsei.ee/en/instructions/applying-for-an-id-card-for-an-adult\">Additional information</a>."));
	else if(!t.isNull())
	{
		setText(QT_TR_NOOP("The card in the card reader is not an Estonian ID-card"));
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
