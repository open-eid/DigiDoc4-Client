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

#include "InfoStack.h"
#include "ui_InfoStack.h"

#include "Styles.h"
#include "SslCertificate.h"

#include <common/DateTime.h>

#include <QtCore/QTextStream>

InfoStack::InfoStack( QWidget *parent )
	: StyledWidget( parent )
	, ui( new Ui::InfoStack )
{
	ui->setupUi( this );

	ui->btnPicture->setFont( Styles::font( Styles::Condensed, 12 ) );
	ui->btnPicture->hide();
	connect( ui->btnPicture, &QPushButton::clicked, this, [this] { emit photoClicked(ui->photo->pixmap() ? *ui->photo->pixmap() : QPixmap()); } );
	
	QFont labelFont = Styles::font(Styles::Condensed, 11);
	ui->labelGivenNames->setFont(labelFont);
	ui->labelSurname->setFont(labelFont);
	ui->labelPersonalCode->setFont(labelFont);
	ui->labelCitizenship->setFont(labelFont);
	ui->labelSerialNumber->setFont(labelFont);

	QFont font = Styles::font(Styles::Regular, 16);
	font.setWeight(QFont::DemiBold);
	ui->valueGivenNames->setFont(font);
	ui->valueSurname->setFont(font);
	ui->valuePersonalCode->setFont(font);
	ui->valueCitizenship->setFont(font);
	ui->valueSerialNumber->setFont(font);
	ui->valueSpacer->setFont(font);
	ui->valueExpiryDate->setFont( Styles::font( Styles::Regular, 16 ) );

	ui->photo->installEventFilter(this);
}

InfoStack::~InfoStack()
{
	delete ui;
}

void InfoStack::clearData()
{
	data = QSmartCardData();
	ui->alternateIcon->hide();
	ui->valueGivenNames->clear();
	ui->valueSurname->clear();
	ui->valuePersonalCode->clear();
	ui->valueCitizenship->clear();
	ui->valueSerialNumber->clear();
	ui->valueSpacer->hide();
	ui->valueExpiryDate->clear();
	clearPicture();
	ui->btnPicture->hide();
}

void InfoStack::clearPicture()
{
	ui->photo->clear();
	ui->btnPicture->setText(tr(pictureText = "DOWNLOAD"));
}

void InfoStack::changeEvent(QEvent* event)
{
	if (event->type() == QEvent::LanguageChange)
	{
		ui->retranslateUi(this);
		update();
	}

	QWidget::changeEvent(event);
}


bool InfoStack::eventFilter(QObject *o, QEvent *e)
{
	if(ui->photo != o || ui->alternateIcon->isVisible() || !ui->photo->pixmap())
		return StyledWidget::eventFilter(o, e);
	switch(e->type())
	{
	case QEvent::Enter:
		ui->btnPicture->show();
		return true;
	case QEvent::Leave:
		ui->btnPicture->hide();
		return true;
	default: return StyledWidget::eventFilter(o, e);
	}
}

void InfoStack::showPicture( const QPixmap &pixmap )
{
	ui->alternateIcon->hide();
	ui->photo->setPixmap( pixmap.scaled( 120, 150, Qt::IgnoreAspectRatio, Qt::SmoothTransformation ) );
	tr("SAVE THE PICTURE");
	ui->btnPicture->setText(tr(pictureText = "SAVE THE PICTURE"));
	ui->btnPicture->hide();
}

void InfoStack::update()
{
	QString text;
	QTextStream st( &text );

	if(certType & SslCertificate::EstEidType)
	{
		if(data.isValid())
			st << "<span style='color: #37a447'>" << tr("Valid") << "</span>" << tr("until")
				<< DateTime(data.data(QSmartCardData::Expiry).toDateTime()).formatDate(QStringLiteral("dd.MM.yyyy"));
		else
			st << "<span style='color: #e80303;'>" << tr("Expired") << "</span>";
	}
	else if(certType & SslCertificate::DigiIDType)
		st << tr("You're using Digital identity card");

	ui->valueGivenNames->setText(givenNamesText);
	ui->valueSurname->setText(surnameText);
	ui->valuePersonalCode->setText(personalCodeText);
	ui->valueCitizenship->setText(citizenshipText);
	ui->valueCitizenship->setHidden(citizenshipText.isEmpty());
	ui->labelCitizenship->setHidden(citizenshipText.isEmpty());
	ui->valueExpiryDate->setText(text);
	ui->valueSerialNumber->setText(serialNumberText);

	ui->alternateIcon->setVisible(certType & SslCertificate::EResidentSubType || certType & SslCertificate::TempelType);
	ui->btnPicture->setText(tr(pictureText));
	if(!ui->photo->pixmap())
		ui->btnPicture->setVisible(ui->alternateIcon->isHidden());
	ui->labelSerialNumber->setHidden(certType & SslCertificate::TempelType);
	ui->valueSerialNumber->setHidden(certType & SslCertificate::TempelType);
	if(certType & SslCertificate::EResidentSubType)
	{
		ui->alternateIcon->load(QStringLiteral(":/images/icon_person_blue.svg"));
		ui->alternateIcon->resize(109, 118);
		ui->alternateIcon->move(10, 16);
	}
	if(certType & SslCertificate::TempelType)
	{
		ui->alternateIcon->load(QStringLiteral(":/images/icon_digitempel.svg"));
		ui->alternateIcon->resize(100, 100);
		ui->alternateIcon->move(10, 25);

		ui->labelGivenNames->setText(tr("NAME"));
		ui->labelSurname->clear();
		ui->labelPersonalCode->setText(tr("SERIAL"));
		ui->labelCitizenship->setText(tr("COUNTRY"));
	}
	else
	{
		ui->labelGivenNames->setText(tr("GIVEN NAMES"));
		ui->labelSurname->setText(tr("SURNAME"));
		ui->labelPersonalCode->setText(tr("PERSONAL CODE"));
		ui->labelCitizenship->setText(tr("CITIZENSHIP"));
		ui->labelSerialNumber->setText(tr("DOCUMENT"));
	}
	ui->valueSpacer->setHidden(ui->valueExpiryDate->text().isEmpty() || ui->valueSerialNumber->text().isEmpty());
}

void InfoStack::update(const SslCertificate &cert)
{
	clearPicture();
	certType = cert.type();
	givenNamesText = cert.toString(QStringLiteral("CN"));
	surnameText.clear();
	serialNumberText.clear();
	personalCodeText = cert.personalCode();
	citizenshipText = cert.toString(QStringLiteral("C"));
	update();
}

void InfoStack::update(const QSmartCardData &t)
{
	if(data != t)
		clearPicture();
	data = t;
	QStringList firstName {
		t.data( QSmartCardData::FirstName1 ).toString(),
		t.data( QSmartCardData::FirstName2 ).toString()
	};
	firstName.removeAll({});

	certType = t.authCert().type();
	givenNamesText = firstName.join(' ');
	surnameText = t.data(QSmartCardData::SurName).toString();
	personalCodeText = t.data(QSmartCardData::Id).toString();
	citizenshipText = t.data(QSmartCardData::Citizen).toString();
	serialNumberText = t.data(QSmartCardData::DocumentId).toString();
	update();
}
