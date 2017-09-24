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

VerifyCert::VerifyCert(QWidget *parent) :
	QWidget(parent),
	ui(new Ui::VerifyCert),
	isValid(false)
{
	ui->setupUi(this);

	ui->name->setFont( Styles::font( Styles::Regular, 18 ) );
	ui->validUntil->setFont( Styles::font( Styles::Regular, 14 ) );
	ui->error->setFont( Styles::font( Styles::Regular, 12, QFont::DemiBold ) );
	QFont regular12 = Styles::font( Styles::Regular, 12 );
	ui->forgotPinLink->setFont( regular12 );
	ui->details->setFont( regular12 );
	borders = " border: solid #DFE5E9; border-width: 1px 0px 0px 0px; ";

    connect(ui->changePIN, &QPushButton::clicked, this, [this](){ emit changePinClicked(); } );
}

VerifyCert::~VerifyCert()
{
	delete ui;
}

void VerifyCert::addBorders()
{
	// Add top, right and left border shadows
	borders = " border: solid #DFE5E9; border-width: 1px 1px 0px 1px; ";
}

void VerifyCert::update(bool isValid, const QString &name, const QString &validUntil, const QString &change, const QString &forgotPinText, const QString &detailsText, const QString &error)
{
	this->isValid = isValid;

	if(isValid)
	{
		this->setStyleSheet("background-color: #ffffff;" + borders);
		ui->verticalSpacerAboveBtn->changeSize( 20, 32 );
		ui->verticalSpacerBelowBtn->changeSize( 20, 38 );
		changePinStyle( "#FFFFFF" );
	}
	else
	{
		this->setStyleSheet(
					"opacity: 0.25;"
					"background-color: #FDF6E9;"  + borders );
		ui->verticalSpacerAboveBtn->changeSize( 20, 8 );
		ui->verticalSpacerBelowBtn->changeSize( 20, 6 );
		ui->changePIN->setStyleSheet(
					"QPushButton { border-radius: 2px; border: none; color: #ffffff; background-color: #006EB5;}"
					"QPushButton:pressed { background-color: #41B6E6;}"
					"QPushButton:hover:!pressed { background-color: #008DCF;}"
					"QPushButton:disabled { background-color: #BEDBED;};"
					);
	}

	if(isValid)
	{
		ui->name->setText(name);
	}
	else
	{
		ui->name->setTextFormat(Qt::RichText);
		ui->name->setText(name + " <img src=\":/images/icon_alert_orange.svg\" height=\"12\" width=\"13\">");
	}
	ui->validUntil->setText(validUntil);
	ui->error->setVisible(!isValid);
	ui->error->setText(error);
	ui->changePIN->setText(change);

	if(forgotPinText.isEmpty())
	{
		ui->forgotPinLink->setVisible(false);
	}
	else
	{
		ui->forgotPinLink->setVisible(true);
		ui->forgotPinLink->setText(forgotPinText);
		ui->forgotPinLink->setOpenExternalLinks(true);
	}

	if(detailsText.isEmpty())
	{
		ui->details->setVisible(false);
	}
	else
	{
		ui->details->setText(detailsText);
		ui->details->setVisible(true);
		ui->details->setOpenExternalLinks(true);
	}
}

void VerifyCert::enterEvent(QEvent * event)
{
	if(isValid)
	{
		this->setStyleSheet("background-color: #f7f7f7;" + borders);
		changePinStyle( "#f7f7f7" );
	}
}

void VerifyCert::leaveEvent(QEvent * event)
{
	if(isValid)
	{
		this->setStyleSheet("background-color: white;" + borders);
		changePinStyle( "white" );
	}
}

void VerifyCert::changePinStyle( const QString &background )
{
	ui->changePIN->setStyleSheet(
		QString("QPushButton { border-radius: 2px; border: 1px solid #006EB5; color: #006EB5; background-color: %1;}"
		"QPushButton:pressed { border: 1px solid #41B6E6; color: #41B6E6;}"
		"QPushButton:hover:!pressed { border: 1px solid #008DCF; color: #008DCF;}"
		"QPushButton:disabled { border: 1px solid #BEDBED; color: #BEDBED;};").arg( background )
		);
}

// Needed to setStyleSheet() take effect.
void VerifyCert::paintEvent(QPaintEvent *)
{
	QStyleOption opt;
	opt.init(this);
	QPainter p(this);
	style()->drawPrimitive(QStyle::PE_Widget, &opt, &p, this);
}
