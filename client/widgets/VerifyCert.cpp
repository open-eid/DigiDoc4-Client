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
#include "dialogs/CertificateDetails.h"
#include "dialogs/WarningDialog.h"

#include <common/DateTime.h>

#include <QtCore/QTextStream>
#include <QtNetwork/QSslKey>

VerifyCert::VerifyCert(QWidget *parent)
	: StyledWidget(parent)
	, ui(new Ui::VerifyCert)
	, borders(QStringLiteral(" border: solid #DFE5E9; border-width: 1px 0px 0px 0px; "))
{
	ui->setupUi( this );

	connect(ui->changePIN, &QPushButton::clicked, [=] {
		emit changePinClicked( false, isBlockedPin );
	});
	connect(ui->forgotPinLink, &QPushButton::clicked, [=] {
		emit changePinClicked( true, false );	// Change PIN with PUK code
	});
	connect(ui->details, &QPushButton::clicked, [=] {
		CertificateDetails::showCertificate(c, this,
			pinType == QSmartCardData::Pin1Type ? QStringLiteral("-auth") : QStringLiteral("-sign"));
	});
	connect(ui->checkCert, &QPushButton::clicked, this, [=]{
		QString msg = tr("Read more <a href=\"https://www.id.ee/en/article/validity-of-id-card-certificates/\">here</a>.");;
		switch(c.validateOnline())
		{
		case SslCertificate::Good:
			msg.prepend(getGoodCertMessage(c));
			break;
		case SslCertificate::Revoked:
			msg.prepend(getRevokedCertMessage(c));
			break;
		default:
			msg = tr("Connecting to SK server failed! Please check your internet connection.");
		}
		WarningDialog::warning(this, msg);
	});

	ui->greenIcon->load(QStringLiteral(":/images/icon_check.svg"));
	ui->orangeIcon->load(QStringLiteral(":/images/icon_alert_orange.svg"));
	ui->redIcon->load(QStringLiteral(":/images/icon_alert_red.svg"));
	ui->greenIcon->hide();
	ui->orangeIcon->hide();
	ui->redIcon->hide();

	ui->name->setFont( Styles::font( Styles::Regular, 18, QFont::Bold  ) );
	ui->validUntil->setFont( Styles::font( Styles::Regular, 14 ) );
	ui->error->setFont( Styles::font( Styles::Regular, 12, QFont::DemiBold ) );
	QFont regular12 = Styles::font( Styles::Regular, 12 );
	regular12.setUnderline(true);
	ui->forgotPinLink->setFont( regular12 );
	ui->forgotPinLink->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
	ui->widgetLayout->setAlignment(ui->forgotPinLink, Qt::AlignCenter);
	ui->details->setFont( regular12 );
	ui->details->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
	ui->widgetLayout->setAlignment(ui->details, Qt::AlignCenter);
	ui->checkCert->setFont(regular12);
	ui->checkCert->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
	ui->widgetLayout->setAlignment(ui->checkCert, Qt::AlignCenter);
	ui->changePIN->setFont( Styles::font( Styles::Condensed, 14 ) );
	ui->tempelText->setFont( Styles::font( Styles::Regular, 14 ) );
	ui->tempelText->hide();
}

VerifyCert::~VerifyCert()
{
	delete ui;
}

void VerifyCert::addBorders()
{
	// Add top, right and left border shadows
	borders = QStringLiteral(" border: solid #DFE5E9; border-width: 1px 1px 0px 1px; ");
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
	bool isRevoked = pinType != QSmartCardData::PukType &&
			cardData.version() >= QSmartCardData::VER_3_5 &&
			cardData.version() < QSmartCardData::VER_IDEMIA &&
			cardData.authCert().publicKey().algorithm() == QSsl::Rsa;
	isValidCert = c.isNull() || (c.isValid() && !isRevoked);

	QString txt;
	QTextStream cert( &txt );

	if(isRevoked)
		cert << tr("Certificate is revoked!");
	else if( !isValidCert )
		cert << tr("Certificate has expired!");
	else
	{
		qint64 leftDays = std::max<qint64>(0, QDateTime::currentDateTime().daysTo(c.expiryDate().toLocalTime()));
		if(leftDays <= 105 && !c.isNull())
			cert << "<span style='color: #F0BF72'>";
		cert << tr("Certificate %1is valid%2 until %3").arg(
				QStringLiteral("<span style='color: #37a447'>"),
				QStringLiteral("</span>"),
				DateTime(c.expiryDate().toLocalTime()).formatDate(QStringLiteral("dd. MMMM yyyy")));
		if(leftDays <= 105 && !c.isNull())
			cert << "</span>";
		if(!isTempelType && cardData.version() != QSmartCardData::VER_IDEMIA)
		{
			if(pinType == QSmartCardData::Pin1Type)
				cert << "<br />" << tr("key has been used %1 times", "pin1").arg(cardData.usageCount(pinType));
			else
				cert << "<br />" << tr("key has been used %1 times", "pin2").arg(cardData.usageCount(pinType));
		}
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
		ui->changePIN->setText(isBlockedPin ? tr("UNBLOCK") : tr("CHANGE PIN%1").arg(pinType));
		ui->changePIN->setHidden((isBlockedPin && isBlockedPuk) || isTempelType);
		ui->forgotPinLink->setText(tr("Forgot PIN%1?").arg(pinType));
		ui->forgotPinLink->setHidden(isBlockedPin || isBlockedPuk || isTempelType);
		ui->checkCert->setVisible(isValidCert);
		ui->error->setText(
			isRevoked ? tr("PIN%1 can not be used because the certificate has revoked. "
				"You can find instructions on how to get a new document from <a href=\"https://www.politsei.ee/en/\"><span style=\"color: #006EB5; text-decoration: none;\">here</span></a>.").arg(pinType) :
			!isValidCert ? tr("PIN%1 can not be used because the certificate has expired.").arg(pinType) :
			(isBlockedPin && isBlockedPuk) ? tr("PIN%1 has been blocked because PIN%1 code has been entered incorrectly 3 times.").arg(pinType) :
			isBlockedPin ? QStringLiteral("%1 %2").arg(tr("PIN%1 has been blocked because PIN%1 code has been entered incorrectly 3 times.").arg(pinType), tr("Unblock to reuse PIN%1.").arg(pinType)) :
			QString()
		);
		break;
	case QSmartCardData::PukType:
		ui->name->setText(tr("PUK code"));
		ui->validUntil->setText(tr("The PUK code is located in your envelope"));
		ui->validUntil->setHidden(isBlockedPuk);
		ui->changePIN->setText(tr("CHANGE PUK"));
		ui->changePIN->setHidden(cardData.version() == QSmartCardData::VER_USABLEUPDATER || isBlockedPuk);
		ui->forgotPinLink->hide();
		ui->details->hide();
		ui->checkCert->hide();
		ui->error->setText(
			isBlockedPin ? tr("PUK code is blocked because the PUK code has been entered 3 times incorrectly. "
				"You can not unblock the PUK code yourself. As long as the PUK code is blocked, all eID options can be used, except PUK code. "
				"Please visit the service center to obtain new codes. <a href=\"https://www.politsei.ee/en/instructions/applying-for-an-id-card-for-an-adult/reminders-for-id-card-holders/\">Additional information</a>.") :
			QString()
		);
		break;
	}

	if( !isValidCert && pinType != QSmartCardData::PukType )
	{
		setStyleSheet( "opacity: 0.25; background-color: #F9EBEB;"  + borders );
		ui->changePIN->setStyleSheet(
					"QPushButton { border-radius: 2px; border: none; color: #ffffff; background-color: #006EB5;}"
					"QPushButton:pressed { background-color: #41B6E6;}"
					"QPushButton:hover:!pressed { background-color: #008DCF;}" 
					"QPushButton:disabled { background-color: #BEDBED;};" 
					); 
		ui->error->setStyleSheet(
					"padding: 6px 6px 6px 6px;"
					"line-height: 14px;"
					"border: 1px solid #c53e3e;"
					"border-radius: 2px;"
					"background-color: #e09797;"
					"color: #5c1c1c;"
					);
	}
	else if( isBlockedPin )
	{
		setStyleSheet( "opacity: 0.25; background-color: #fcf5ea;"  + borders );
		ui->changePIN->setStyleSheet(
					"QPushButton { border-radius: 2px; border: none; color: #ffffff; background-color: #006EB5;}" 
					"QPushButton:pressed { background-color: #41B6E6;}" 
					"QPushButton:hover:!pressed { background-color: #008DCF;}" 
					"QPushButton:disabled { background-color: #BEDBED;};" 
					); 
		ui->error->setStyleSheet(
					"padding: 6px 6px 6px 6px;"
					"line-height: 14px;"
					"border: 1px solid #e89c30;"
					"border-radius: 2px;"
					"background-color: #F8DDA7;"
					);
	}
	else
	{
		setStyleSheet( "background-color: #ffffff;" + borders );
		changePinStyle(QStringLiteral("#FFFFFF"));
	}
	ui->redIcon->setVisible(!isValidCert && pinType != QSmartCardData::PukType);
	ui->orangeIcon->setVisible(ui->redIcon->isHidden() && isBlockedPin);
	ui->greenIcon->setVisible(ui->redIcon->isHidden() && ui->orangeIcon->isHidden() && pinType != QSmartCardData::PukType);
	ui->error->setHidden(ui->error->text().isEmpty());
	ui->tempelText->setVisible(isTempelType);
	ui->changePIN->setAccessibleName(ui->changePIN->text().toLower());
}

bool VerifyCert::event(QEvent *event)
{
	switch(event->type())
	{
	case QEvent::Enter:
		if( !isValidCert && pinType != QSmartCardData::PukType )
			setStyleSheet("background-color: #f3d8d8;" + borders);
		else if( isBlockedPin )
			setStyleSheet("background-color: #fbecd0;" + borders);
		else
		{
			setStyleSheet("background-color: #f7f7f7;" + borders);
			changePinStyle(QStringLiteral("#f7f7f7"));
		}
		break;
	case QEvent::Leave:
		if( !isValidCert && pinType != QSmartCardData::PukType )
			setStyleSheet("background-color: #F9EBEB;" + borders);
		else if( isBlockedPin )
			setStyleSheet("background-color: #fcf5ea;" + borders);
		else
		{
			setStyleSheet( "background-color: white;" + borders);
			changePinStyle(QStringLiteral("white"));
		}
		break;
	case QEvent::LanguageChange:
		ui->retranslateUi(this);
		update();
		break;
	default: break;
	}
	return StyledWidget::event(event);
}

void VerifyCert::changePinStyle( const QString &background )  
{
	ui->changePIN->setStyleSheet(QStringLiteral(
		"QPushButton { border-radius: 2px; border: 1px solid #006EB5; color: #006EB5; background-color: %1;}"
		"QPushButton:pressed { border: none; background-color: #006EB5; color: %1;}"
		"QPushButton:hover:!pressed { border-radius: 2px; border: 1px solid #008DCF; color: #008DCF;}"
		"QPushButton:disabled { border: 1px solid #BEDBED; color: #BEDBED;};").arg( background )
	);
}

QString VerifyCert::getGoodCertMessage(const SslCertificate& cert) {
	if (SslCertificate::CertType::TempelType == cert.type()) {
		return tr("Certificate is valid. ");
	}

	return cert.keyUsage().contains(SslCertificate::NonRepudiation) ?
		tr("Your ID-card signing certificate is valid. ") :
		tr("Your ID-card authentication certificate is valid. ");
}
QString VerifyCert::getRevokedCertMessage(const SslCertificate& cert) {
	if (SslCertificate::CertType::TempelType == cert.type()) {
		return tr("Certificate is not valid. A valid certificate is required for electronic use. ");
	}

	return cert.keyUsage().contains(SslCertificate::NonRepudiation) ?
		tr("Your ID-card signing certificate is not valid. You need valid certificates to use your ID-card electronically. ") :
		tr("Your ID-card authentication certificate is not valid. You need valid certificates to use your ID-card electronically. ");
}

