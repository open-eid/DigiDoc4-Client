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
#include "effects/HoverFilter.h"

#include <common/SslCertificate.h>

#include <QtCore/QTextStream>

InfoStack::InfoStack( QWidget *parent )
: StyledWidget( parent )
, ui( new Ui::InfoStack )
, appletVersion()
, certTypeIsEstEid(false)
, certIsValid(false)
, citizenshipText()
, expireDate()
, givenNamesText()
, personalCodeText()
, serialNumberText()
, surnameText()
, pictureText("DOWNLOAD")
{
	ui->setupUi( this );

	ui->btnPicture->setFont( Styles::font( Styles::Condensed, 12 ) );
	connect( ui->btnPicture, &QPushButton::clicked, this, [this]() { emit photoClicked( ui->photo->pixmap() ); } );
	
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
	ui->valueExpiryDate->setFont( Styles::font( Styles::Regular, 16 ) );

	HoverFilter *filter = new HoverFilter(ui->photo, [this](int eventType){ focusEvent(eventType); }, this);
	ui->photo->installEventFilter(filter);
}

InfoStack::~InfoStack()
{
	delete ui;
}

void InfoStack::clearPicture()
{
	ui->photo->clear();
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


void InfoStack::focusEvent(int eventType)
{
	if(!ui->photo->pixmap())
		return;

	if(eventType == QEvent::Enter)
	{
		ui->btnPicture->show();
	}
	else
	{
		// Ignore multiple enter-leave events sent when moving over button
		auto boundingRect = QRect(ui->photo->mapToGlobal(ui->photo->geometry().topLeft()),
								  ui->photo->mapToGlobal(ui->photo->geometry().bottomRight()));
		if(!boundingRect.contains(QCursor::pos()))
			ui->btnPicture->hide();
	}
}

void InfoStack::update()
{
	QString text;
	QTextStream st( &text );

	if(certTypeIsEstEid)
	{
		if(certIsValid)
			st << "<span style='color: #37a447'>" << tr("Valid") << "</span>" << tr("until") << expireDate;
		else
			st << "<span style='color: #e80303;'>" << tr("Expired") << "</span>";
	}
	else
	{
		st << tr("You're using Digital identity card"); // ToDo
	}

	ui->valueGivenNames->setText(givenNamesText);
	ui->valueSurname->setText(surnameText);
	ui->valuePersonalCode->setText(personalCodeText);
	ui->valueCitizenship->setText(citizenshipText);
	ui->valueExpiryDate->setText(text);
	ui->valueSerialNumber->setText(serialNumberText);
	ui->btnPicture->setText(tr(qPrintable(pictureText)));
}

void InfoStack::update(const QSmartCardData &t)
{
	QStringList firstName = QStringList()
		<< t.data( QSmartCardData::FirstName1 ).toString()
		<< t.data( QSmartCardData::FirstName2 ).toString();
	firstName.removeAll( "" );

	certTypeIsEstEid = t.authCert().type() & SslCertificate::EstEidType;
	certIsValid = t.isValid();
	expireDate = DateTime(t.data(QSmartCardData::Expiry).toDateTime()).formatDate( "dd.MM.yyyy" );
	givenNamesText = firstName.join(" ");
	surnameText = t.data(QSmartCardData::SurName).toString();
	personalCodeText = t.data(QSmartCardData::Id).toString();
	citizenshipText = t.data(QSmartCardData::Citizen).toString();
	serialNumberText = t.data(QSmartCardData::DocumentId).toString();
	if(!serialNumberText.isEmpty())
		serialNumberText += "  |" ;

	update();
}

void InfoStack::clearData()
{
	ui->valueGivenNames->setText( "" );
	ui->valueSurname->setText( "" );
	ui->valuePersonalCode->setText( "" );
	ui->valueCitizenship->setText( "" );
	ui->valueSerialNumber->setText( "" );
	ui->valueExpiryDate->setText( "" );
	pictureText = "DOWNLOAD";
	ui->btnPicture->setText(tr(qPrintable(pictureText)));
	ui->btnPicture->show();
}

void InfoStack::showPicture( const QPixmap &pixmap )
{
	ui->photo->setProperty( "PICTURE", pixmap );
	ui->photo->setPixmap( pixmap.scaled( 120, 150, Qt::IgnoreAspectRatio, Qt::SmoothTransformation ) );
	pictureText = "SAVE THE PICTURE";
	ui->btnPicture->setText(tr(qPrintable(pictureText)));
	ui->btnPicture->hide();
}
