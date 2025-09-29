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

#include "DateTime.h"
#include "dialogs/CertificateDetails.h"
#include "dialogs/WarningDialog.h"

#include <QtCore/QTextStream>
#include <QtNetwork/QSslKey>

VerifyCert::VerifyCert(QWidget *parent)
	: StyledWidget(parent)
	, ui(new Ui::VerifyCert)
{
	ui->setupUi( this );

	connect(ui->changePIN, &QPushButton::clicked, this, [this] {
		emit changePinClicked( false, isBlockedPin );
	});
	connect(ui->forgotPinLink, &QPushButton::clicked, this, [this] {
		emit changePinClicked( true, false );	// Change PIN with PUK code
	});
	connect(ui->details, &QPushButton::clicked, this, [this] {
		CertificateDetails::showCertificate(c, this,
			pinType == QSmartCardData::Pin1Type ? QStringLiteral("-auth") : QStringLiteral("-sign"));
	});
	connect(ui->checkCert, &QPushButton::clicked, this, [this]{
		QString msg = tr("Read more <a href=\"https://www.id.ee/en/article/validity-of-id-card-certificates/\">here</a>.");
		switch(c.validateOnline())
		{
		case SslCertificate::Good:
			if(SslCertificate::CertType::TempelType == c.type())
				msg.prepend(tr("Certificate is valid. "));
			else if(c.keyUsage().contains(SslCertificate::NonRepudiation))
				msg.prepend(tr("Your ID-card signing certificate is valid. "));
			else
				msg.prepend(tr("Your ID-card authentication certificate is valid. "));
			break;
		case SslCertificate::Revoked:
			if(SslCertificate::CertType::TempelType == c.type())
				msg.prepend(tr("Certificate is not valid. A valid certificate is required for electronic use. "));
			else if(c.keyUsage().contains(SslCertificate::NonRepudiation))
				msg.prepend(tr("Your ID-card signing certificate is not valid. You need valid certificates to use your ID-card electronically. "));
			else
				msg.prepend(tr("Your ID-card authentication certificate is not valid. You need valid certificates to use your ID-card electronically. "));
			break;
		default:
			msg = tr("Certificate status check failed. Please check your internet connection.");
		}
		WarningDialog::show(this, msg);
	});

	ui->nameIcon->load(QStringLiteral(":/images/icon_alert_red.svg"));
	ui->nameIcon->hide();
	ui->infoText->hide();
}

VerifyCert::~VerifyCert()
{
	delete ui;
}

void VerifyCert::clear()
{
	c = SslCertificate();
	isBlockedPin = false;
	cardData = QSmartCardData();
	update();
}

void VerifyCert::update(QSmartCardData::PinType type, const QSmartCardData &data)
{
	pinType = type;
	cardData = data;
	c = (pinType == QSmartCardData::Pin1Type) ? cardData.authCert() : cardData.signCert();
	isBlockedPin = cardData.retryCount( pinType ) == 0;
	update();
}

void VerifyCert::update(QSmartCardData::PinType type, const SslCertificate &cert)
{
	pinType = type;
	c = cert;
	isBlockedPin = false;
	update();
}

void VerifyCert::update()
{
	bool isBlockedPuk = !cardData.isNull() && cardData.retryCount( QSmartCardData::PukType ) == 0;
	bool isTempelType = c.type() & SslCertificate::TempelType;
	isValidCert = c.isNull() || c.isValid();
	ui->error->clear();
	setStyleSheet(QStringLiteral("background-color: #ffffff;")); // Fixes layout issue when PIN change is clicked

	QString txt;
	QTextStream cert( &txt );

	if(!isValidCert)
		cert << tr("Certificate has expired!");
	else
	{
		qint64 leftDays = std::max<qint64>(0, QDateTime::currentDateTime().daysTo(c.expiryDate().toLocalTime()));
		if(leftDays <= 105 && !c.isNull())
			cert << "<span style='color: #996C0B'>";
		cert << tr("Certificate %1is valid%2 until %3").arg(
				QLatin1String("<span style='color: #498526'>"),
				QLatin1String("</span>"),
				DateTime(c.expiryDate().toLocalTime()).formatDate(QStringLiteral("dd. MMMM yyyy")));
		if(leftDays <= 105 && !c.isNull())
			cert << "</span>";
	}
	switch(pinType)
	{
	case QSmartCardData::Pin1Type:
	case QSmartCardData::Pin2Type:
		if(pinType == QSmartCardData::Pin2Type)
			ui->name->setText(tr("Signing certificate"));
		else if(c.enhancedKeyUsage().contains(SslCertificate::ClientAuth))
			ui->name->setText(tr("Authentication certificate"));
		else
			ui->name->setText(tr("Certificate for Encryption"));
		ui->validUntil->setText(txt);
		ui->changePIN->setText(isBlockedPin ? tr("Unblock") : tr("Change PIN%1").arg(pinType));
		ui->changePIN->setHidden((isBlockedPin && isBlockedPuk) || isTempelType);
		ui->infoText->setVisible(isTempelType);
		ui->forgotPinLink->setText(tr("Forgot PIN%1?").arg(pinType));
		ui->checkCert->setVisible(isValidCert);
		if(!isValidCert)
			ui->error->setText(tr("PIN%1 can not be used because the certificate has expired.").arg(pinType));
		else if(isBlockedPin)
			ui->error->setText(tr("PIN%1 has been blocked because PIN%1 code has been entered incorrectly 3 times.").arg(pinType) +
				(isBlockedPuk ? ' ' + tr("Unblock to reuse PIN%1.").arg(pinType) : QString()));
		break;
	case QSmartCardData::PukType:
		ui->name->setText(tr("PUK code"));
		ui->validUntil->setText(tr("The PUK code is located in your envelope"));
		ui->validUntil->setHidden(isBlockedPuk);
		ui->changePIN->setText(tr("Change PUK"));
		ui->changePIN->setHidden(isBlockedPuk || !cardData.isPUKReplacable());
		ui->infoText->setText(tr("The PUK code cannot be changed on the ID-card in the reader.<br />"
			"If you have forgotten the PUK code of your ID-card then you can view it from the Police and Border Guard Board portal.<br />"
			"<a href=\"https://www.id.ee/en/article/pin-and-puk-codes-security-recommendations/\">Additional information</a>."));
		ui->infoText->setVisible(!cardData.isPUKReplacable());
		ui->details->hide();
		ui->checkCert->hide();
		if(isBlockedPuk)
			ui->error->setText(tr("PUK code is blocked because the PUK code has been entered 3 times incorrectly.<br/>"
				"You can not unblock the PUK code yourself.<br/>As long as the PUK code is blocked, all eID options can be used, except PUK-code.<br/>") +
				(cardData.isPUKReplacable() ?
				tr("Please visit the service center to obtain new codes.<br/>"
					"<a href=\"https://www.politsei.ee/en/instructions/applying-for-an-id-card-for-an-adult/reminders-for-id-card-holders/\">Additional information</a>.") :
				tr("A new document must be requested to receive the new PUK code.<br/>"
					"<a href=\"https://www.politsei.ee/en/instructions/applying-for-an-id-card-for-an-adult\">Additional information</a>."))
			);

		ui->widget->setHidden(isBlockedPuk);
		break;
	}

	if( !isValidCert && pinType != QSmartCardData::PukType )
	{
		ui->error->setStyleSheet(QStringLiteral(
			"padding: 6px 6px 6px 6px;"
			"line-height: 14px;"
			"border: 1px solid #c53e3e;"
			"border-radius: 2px;"
			"background-color: #e09797;"
			"color: #5c1c1c;"));
		ui->nameIcon->show();
	}
	else if( isBlockedPin )
	{
		ui->error->setStyleSheet(QStringLiteral(
			"padding: 6px 6px 6px 6px;"
			"line-height: 14px;"
			"border: 1px solid #e89c30;"
			"border-radius: 2px;"
			"background-color: #F8DDA7;"));
		ui->nameIcon->show();
	}
	else
		ui->nameIcon->hide();
	ui->error->setHidden(ui->error->text().isEmpty());
	ui->changePIN->setDefault(isBlockedPin);
	ui->changePIN->setAccessibleName(ui->changePIN->text().toLower());
	ui->forgotPinLink->setHidden(isBlockedPin || isBlockedPuk || isTempelType || pinType == QSmartCardData::PukType);

	if(pinType == QSmartCardData::Pin1Type)
	{
		adjustSize();
		if(auto *pukBox = parent()->findChild<VerifyCert*>(QStringLiteral("pukBox")))
			pukBox->ui->validUntil->setMinimumSize(ui->validUntil->size());
	}
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
