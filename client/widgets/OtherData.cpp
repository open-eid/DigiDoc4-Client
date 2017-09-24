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

#include "OtherData.h"
#include "ui_OtherData.h"
#include "Styles.h"

OtherData::OtherData(QWidget *parent) :
	QWidget(parent),
	ui(new Ui::OtherData)
{
	ui->setupUi(this);

	connect( ui->inputEMail, &QLineEdit::textChanged, this, [this](){ update(true); } );
	connect( ui->btnCheckEMail, &QPushButton::clicked, this, [this](){ emit checkEMailClicked(); } );
	connect( ui->activate, &QPushButton::clicked, this, [this](){ emit activateEMailClicked(); } );

	QFont font = Styles::font( Styles::Regular, 13 );
	QFont condensed = Styles::font( Styles::Condensed, 14 );
	
	ui->label->setFont( Styles::font( Styles::Regular, 13, QFont::DemiBold ) );
	ui->lblEMail->setFont( font );
	ui->lblNoForwarding->setFont( font );
	ui->labelEestiEe->setFont( Styles::font( Styles::Regular, 12 ) );
	ui->activate->setFont( condensed );
	ui->btnCheckEMail->setFont( condensed );
	ui->activate->setStyleSheet(
			"padding: 6px 9px;"
			"QPushButton { border-radius: 2px; border: none; color: #ffffff; background-color: #006EB5;}"
			"QPushButton:pressed { background-color: #41B6E6;}"
			"QPushButton:hover:!pressed { background-color: #008DCF;}"
			"QPushButton:disabled { background-color: #BEDBED;};"
			);
}

OtherData::~OtherData()
{
	delete ui;
}

void OtherData::update( bool activate, const QString &eMail, const quint8 &errorCode )
{
	if( activate )
	{
		ui->btnCheckEMail->setVisible( false );
		ui->activateEMail->setVisible( true );
		if( !eMail.isEmpty() )
			ui->lblEMail->setText( QString("<b>") + eMail + QString("</b>") );  // Show error text here
		else
			ui->lblEMail->setVisible( false );

		if( ui->inputEMail->text().isEmpty() )
		{
			ui->activate->setDisabled( true );
			ui->activate->setCursor( Qt::ArrowCursor );
		}
		else
		{
			ui->activate->setDisabled( false );
			ui->activate->setCursor( Qt::PointingHandCursor );
		}
		ui->inputEMail->setFocus();
	}
	else
	{
		ui->btnCheckEMail->setVisible( true );
		ui->lblEMail->setVisible( false );
		ui->activateEMail->setVisible( false );

		if( !eMail.isEmpty() )
		{
			ui->btnCheckEMail->setVisible( false );
			ui->lblEMail->setVisible( true );
			ui->activateEMail->setVisible( false );

			if( errorCode )
				ui->lblEMail->setText( QString("<b>") + eMail + QString("</b>") );  // Show error text here
			else
				ui->lblEMail->setText( QString("Teie @eesti.ee posti aadressid on suunatud e-postile <br/><b>") + eMail + QString("</b>") );
		}
	}
}

void OtherData::paintEvent(QPaintEvent *)
{
	QStyleOption opt;
	opt.init(this);
	QPainter p(this);
	style()->drawPrimitive(QStyle::PE_Widget, &opt, &p, this);
}

QString OtherData::getEmail()
{
	return ( ui->inputEMail->text().isEmpty() || ui->inputEMail->text().indexOf( "@" ) == -1 ) ? "" : ui->inputEMail->text();
}

void OtherData::setFocusToEmail()
{
	ui->inputEMail->setFocus();
}
