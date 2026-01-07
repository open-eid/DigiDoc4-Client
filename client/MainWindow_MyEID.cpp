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
#include "widgets/WarningItem.h"
#include "widgets/WarningList.h"

#include <QDateTime>
#include <QtNetwork/QSslKey>

using namespace ria::qdigidoc4;

void MainWindow::changePinClicked(QSmartCardData::PinType type, QSmartCard::PinAction action)
{
	if(qApp->signer()->smartcard()->pinChange(type, action, ui->topBar))
		updateMyEid(qApp->signer()->smartcard()->data());
}

void MainWindow::updateMyEID(const TokenData &t)
{
	ui->warnings->clearMyEIDWarnings();
	SslCertificate cert(t.cert());
	auto type = cert.type();
	ui->infoStack->setHidden(type == SslCertificate::UnknownType);
	ui->accordion->setHidden(type == SslCertificate::UnknownType);
	ui->noReaderInfo->setVisible(type == SslCertificate::UnknownType);

	auto setText = [this](const char *text) {
		ui->noReaderInfoText->setProperty("currenttext", text);
		ui->noReaderInfoText->setText(tr(text));
	};
	if(!t.isNull())
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
		setText(QT_TR_NOOP("Connect the card reader to your computer and insert your ID card into the reader"));
	}
}

void MainWindow::updateMyEid(const QSmartCardData &data)
{
	ui->infoStack->update(data);
	ui->accordion->updateInfo(data);
	ui->myEid->warningIcon(false);
	ui->myEid->invalidIcon(false);
	ui->warnings->clearMyEIDWarnings();
	if(data.isNull())
		return;
	bool pin1Blocked = data.retryCount(QSmartCardData::Pin1Type) == 0;
	bool pin2Blocked = data.retryCount(QSmartCardData::Pin2Type) == 0;
	bool pin2Locked = data.pinLocked(QSmartCardData::Pin2Type);
	ui->myEid->warningIcon(
		pin1Blocked ||
		pin2Blocked || pin2Locked ||
		data.retryCount(QSmartCardData::PukType) == 0);
	ui->signContainerPage->cardChanged(data.signCert(), pin2Blocked || pin2Locked);
	ui->cryptoContainerPage->cardChanged(data.authCert(), pin1Blocked);

	if(pin1Blocked)
		ui->warnings->showWarning({WarningType::UnblockPin1Warning, 0,
			[this]{ changePinClicked(QSmartCardData::Pin1Type, QSmartCard::UnblockWithPuk); }});

	if(pin2Locked && pin2Blocked)
		ui->warnings->showWarning({WarningType::ActivatePin2WithPUKWarning, 0,
			[this]{ changePinClicked(QSmartCardData::Pin2Type, QSmartCard::ActivateWithPuk); }});
	else if(pin2Blocked)
		ui->warnings->showWarning({WarningType::UnblockPin2Warning, 0,
			[this]{ changePinClicked(QSmartCardData::Pin2Type, QSmartCard::UnblockWithPuk); }});
	else if(pin2Locked)
		ui->warnings->showWarning({WarningType::ActivatePin2Warning, 0,
			[this]{ changePinClicked(QSmartCardData::Pin2Type, QSmartCard::ActivateWithPin); }});

	const qint64 DAY = 24 * 60 * 60;
	qint64 expiresIn = 106 * DAY;
	for(const QSslCertificate &cert: {data.authCert(), data.signCert()})
	{
		if(!cert.isNull())
			expiresIn = std::min<qint64>(expiresIn, QDateTime::currentDateTime().secsTo(cert.expiryDate().toLocalTime()));
	}
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
