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

#include "dialogs/CertificateDetails.h"
#include "dialogs/WarningDialog.h"

#include <QtWidgets/QStyle>

VerifyCert::VerifyCert(QWidget *parent)
	: StyledWidget(parent)
	, ui(new Ui::VerifyCert)
{
	ui->setupUi( this );

	connect(ui->changePIN, &QPushButton::clicked, this, [this] {
		if(cardData.retryCount(pinType) == 0 && cardData.pinLocked(pinType))
			emit changePinClicked(QSmartCard::ActivateWithPuk);
		else if(cardData.retryCount(pinType) == 0)
			emit changePinClicked(QSmartCard::UnblockWithPuk);
		else if(cardData.pinLocked(pinType))
			emit changePinClicked(QSmartCard::ActivateWithPin);
		else
			emit changePinClicked(QSmartCard::ChangeWithPin);
	});
	connect(ui->forgotPinLink, &QPushButton::clicked, this, [this] {
		emit changePinClicked(cardData.pinLocked(pinType) ? QSmartCard::ActivateWithPuk : QSmartCard::ChangeWithPuk);
	});
	connect(ui->details, &QPushButton::clicked, this, [this] {
		CertificateDetails::showCertificate(c, this,
			pinType == QSmartCardData::Pin1Type ? QStringLiteral("-auth") : QStringLiteral("-sign"));
	});
	connect(ui->checkCert, &QPushButton::clicked, this, [this]{
		auto *dlg = WarningDialog::create(this);
		QString readMore = tr("Read more <a href=\"https://www.id.ee/en/article/validity-of-id-card-certificates/\">here</a>.");
		switch(c.validateOnline())
		{
		case SslCertificate::Good:
			if(SslCertificate::CertType::TempelType == c.type())
				dlg->withTitle(tr("Certificate is valid"));
			else if(c.keyUsage().contains(SslCertificate::NonRepudiation))
				dlg->withTitle(tr("Your ID-card signing certificate is valid"));
			else
				dlg->withTitle(tr("Your ID-card authentication certificate is valid"));
			break;
		case SslCertificate::Revoked:
			if(SslCertificate::CertType::TempelType == c.type())
				dlg->withTitle(tr("Certificate is not valid"))
					->withText(tr("A valid certificate is required for electronic use. ") + readMore);
			else if(c.keyUsage().contains(SslCertificate::NonRepudiation))
				dlg->withTitle(tr("Your ID-card signing certificate is not valid"))
					->withText(tr("You need valid certificates to use your ID-card electronically. ") + readMore);
			else
				dlg->withTitle(tr("Your ID-card authentication certificate is not valid"))
					->withText(tr("You need valid certificates to use your ID-card electronically. ") + readMore);
			break;
		case SslCertificate::Unknown:
			if(SslCertificate::CertType::TempelType == c.type())
				dlg->withTitle(tr("Certificate status is unknown"))
					->withText(tr("A valid certificate is required for electronic use. ") + readMore);
			else if(c.keyUsage().contains(SslCertificate::NonRepudiation))
				dlg->withTitle(tr("Your ID-card signing certificate status is unknown"))
					->withText(tr("You need valid certificates to use your ID-card electronically. ") + readMore);
			else
				dlg->withTitle(tr("Your ID-card authentication certificate status is unknown"))
					->withText(tr("You need valid certificates to use your ID-card electronically. ") + readMore);
			break;
		default:
			dlg->withTitle(tr("Certificate status check failed"))
				->withText(tr("Please check your internet connection."));
		}
		dlg->open();
	});

	clear();
}

VerifyCert::~VerifyCert()
{
	delete ui;
}

void VerifyCert::clear()
{
	c = SslCertificate();
	cardData = QSmartCardData();
	ui->name->clear();
	ui->nameIcon->hide();
	ui->validUntil->hide();
	ui->info->hide();
	ui->changePIN->hide();
	ui->links->hide();
}

void VerifyCert::update(QSmartCardData::PinType type, const QSmartCardData &data)
{
	pinType = type;
	cardData = data;
	c = (pinType == QSmartCardData::Pin1Type) ? cardData.authCert() : cardData.signCert();
	update();
}

void VerifyCert::update(QSmartCardData::PinType type, const SslCertificate &cert)
{
	pinType = type;
	c = cert;
	update();
}

void VerifyCert::update()
{
	if(cardData.isNull() && c.isNull())
		return clear();
	bool isLockedPin = !cardData.isNull() && pinType == QSmartCardData::Pin2Type && cardData.pinLocked(pinType);
	bool isBlockedPin = !cardData.isNull() && cardData.retryCount(pinType) == 0;
	bool isBlockedPuk = !cardData.isNull() && cardData.retryCount(QSmartCardData::PukType) == 0;
	bool isTempelType = c.type() & SslCertificate::TempelType;
	bool isInvalidCert = !c.isNull() && !c.isValid();
	bool isPUKReplacable = cardData.isPUKReplacable();
	QString icon;
	ui->info->clear();
	ui->validUntil->show();

	if(pinType == QSmartCardData::PukType)
	{
		ui->name->setText(tr("PUK code"));

		ui->validUntil->setLabel({});
		ui->validUntil->setText(tr("The PUK code is located in your envelope"));
		ui->validUntil->setHidden(isBlockedPuk);

		ui->changePIN->setText(tr("Change PUK"));
		ui->changePIN->setHidden(isBlockedPuk || !isPUKReplacable);
		if(isBlockedPuk)
		{
			icon = QStringLiteral(":/images/icon_alert_large_error.svg");
			ui->info->setLabel(QStringLiteral("error"));
			ui->info->setText(tr("PUK code is blocked because the PUK code has been entered 3 times incorrectly.<br/>"
				"You can not unblock the PUK code yourself.<br/>As long as the PUK code is blocked, all eID options can be used, except PUK-code.<br/>") +
				(isPUKReplacable ?
				tr("Please visit the service center to obtain new codes.<br/>"
					"<a href=\"https://www.politsei.ee/en/instructions/applying-for-an-id-card-for-an-adult/reminders-for-id-card-holders/\">Additional information</a>.") :
				tr("A new document must be requested to receive the new PUK code.<br/>"
					"<a href=\"https://www.politsei.ee/en/instructions/applying-for-an-id-card-for-an-adult\">Additional information</a>."))
			);
		}
		else if(!isPUKReplacable)
		{
			ui->info->setLabel({});
			ui->info->setText(tr("The PUK code cannot be changed on the ID-card in the reader.<br />"
				"If you have forgotten the PUK code of your ID-card then you can view it from the Police and Border Guard Board portal.<br />"
				 "<a href=\"https://www.id.ee/en/article/pin-and-puk-codes-security-recommendations/\">Additional information</a>."));
		}
	} else {
		if(pinType == QSmartCardData::Pin2Type)
			ui->name->setText(tr("Signing certificate"));
		else if(c.enhancedKeyUsage().contains(SslCertificate::ClientAuth))
			ui->name->setText(tr("Authentication certificate"));
		else
			ui->name->setText(tr("Certificate for Encryption"));

		ui->validUntil->setText(tr("Certificate is valid until %1").arg(
			c.expiryDate().toLocalTime().toString(QStringLiteral("dd.MM.yyyy"))));
		if(isInvalidCert)
		{
			ui->validUntil->setText(tr("Certificate has expired!"));
			ui->validUntil->setLabel(QStringLiteral("error"));
		}
		else if(qint64 leftDays = std::max<qint64>(0, QDateTime::currentDateTime().daysTo(c.expiryDate().toLocalTime())); leftDays <= 105 && !c.isNull())
		{
			icon = QStringLiteral(":/images/icon_alert_large_warning.svg");
			ui->validUntil->setLabel(QStringLiteral("warning"));
		}
		else
			ui->validUntil->setLabel(QStringLiteral("good"));

		ui->changePIN->setText(tr("Change PIN%1").arg(pinType));
		ui->forgotPinLink->setText(tr("Change with PUK code"));
		ui->changePIN->setHidden((isBlockedPin && isBlockedPuk) || isTempelType);

		if(isTempelType)
		{
			ui->info->setLabel({});
			ui->info->setText(tr("PIN can be changed only using eToken utility"));
		}
		else if(isInvalidCert)
		{
			icon = QStringLiteral(":/images/icon_alert_large_error.svg");
			ui->info->setLabel(QStringLiteral("error"));
			ui->info->setText(tr("PIN%1 can not be used because the certificate has expired.").arg(pinType));
		}
		else if(isBlockedPin)
		{
			ui->changePIN->setText(tr("Unblock"));
			icon = isBlockedPuk ? QStringLiteral(":/images/icon_alert_large_error.svg") : QStringLiteral(":/images/icon_alert_large_warning.svg");
			ui->info->setLabel(isBlockedPuk ? QStringLiteral("error") : QStringLiteral("warning"));
			ui->info->setText(tr("PIN%1 has been blocked because PIN%1 code has been entered incorrectly 3 times.").arg(pinType) +
				(isBlockedPuk ? QString() : ' ' + tr("Unblock to reuse PIN%1.").arg(pinType)));
		}
		else if(isLockedPin)
		{
			icon = QStringLiteral(":/images/icon_alert_large_warning.svg");
			ui->info->setLabel(QStringLiteral("warning"));
			ui->info->setText((pinType == QSmartCardData::Pin1Type ?
				tr("PIN%1 code must be changed in order to authenticate") :
				tr("PIN%1 code must be changed in order to sign")).arg(pinType));
		}

	}

	ui->info->setHidden(ui->info->text().isEmpty());
	ui->changePIN->setDefault(isBlockedPin);
	ui->nameIcon->setHidden(icon.isEmpty());
	if(!icon.isEmpty())
		ui->nameIcon->load(icon);

	ui->links->setHidden(pinType == QSmartCardData::PukType && (isBlockedPuk || !isPUKReplacable)); // Keep visible in PUK to align fields equaly
	ui->details->setHidden(pinType == QSmartCardData::PukType);
	ui->forgotPinLink->setHidden(pinType == QSmartCardData::PukType || isBlockedPin || isBlockedPuk || isTempelType);
	ui->checkCert->setHidden(pinType == QSmartCardData::PukType || isInvalidCert);
}

void VerifyCert::changeEvent(QEvent *event)
{
	if(event->type() == QEvent::LanguageChange)
	{
		ui->retranslateUi(this);
		update();
	}
	StyledWidget::changeEvent(event);
}
