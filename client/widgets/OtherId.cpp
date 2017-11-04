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

#include "OtherId.h"
#include "ui_OtherId.h"

#include "Styles.h"
#include "XmlReader.h"

#include <QtCore/QTextStream>

OtherId::OtherId( QWidget *parent ) :
  StyledWidget( parent )
, ui( new Ui::OtherId )
{
	ui->setupUi( this );

	QFont font11 = Styles::font( Styles::Regular, 11 );
	QFont font14 = Styles::font( Styles::Regular, 14 );
	QFont font18 = Styles::font( Styles::Regular, 18 );

	ui->lblIdName->setFont( font18 );
	ui->lblLeftHdr->setFont( font11 );
	ui->lblLeftText->setFont( font14 );
	ui->lblRightHdr->setFont( font11 );
	ui->lblRightText->setFont( font14 );
	ui->lblStatusHdr->setFont( font11 );
	ui->lblStatusText->setFont( font14 );
	ui->lblCertHdr->setFont( font11 );
	ui->lblCertText->setFont( font14 );
	ui->lblDigiIdInfo->setFont( font11 );
}

OtherId::~OtherId()
{
	delete ui;
}


void OtherId::updateMobileId( const QString &telNumber, const QString &telOperator, const QString &status, const QString &validTill )
{
	this->isDigiId = false;
	this->telNumber = telNumber;
	this->telOperator = telOperator;
	this->docNumber = "";
	this->status = status;
	this->validTill = validTill;

	updateMobileId();
}

void OtherId::updateMobileId()
{
	isDigiId = false;

	QString text;
	QTextStream s( &text );

	ui->lblDigiIdInfo->setText( "" );
	ui->lblIdName->setText( tr( "Mobile ID" ) );
	ui->lblLeftHdr->setText( tr( "PHONE NUMBER" ) );
	ui->lblLeftText->setText( "+" + telNumber );
	ui->lblRightHdr->setText( tr( "MOBILE OPERATOR" ) );
	ui->lblRightText->setText( telOperator );
	if( status == "Active" )
	{
		s << tr( "Certificates are " ) << "<span style='color: #8CC368'>"
			<< tr( "activated" ) << "</span>"
			<< tr( " and Mobile ID using is " ) << "<span style='color: #8CC368'>"
			<< tr( "possible" ) << "</span>";

		ui->lblStatusText->setText( text );
		ui->lblCertText->setText( "<span style='color: #8CC368'>" + tr("Valid") + "</span> kuni " + validTill );
	}
	else
	{
		s << "<span style='color: #e80303'>"
			<< XmlReader::mobileStatus( status ) << "</span>";
		ui->lblStatusText->setText( text );
		ui->lblCertText->setText( "" );
	}
}

void OtherId::updateDigiId( const QString &docNumber, const QString &docValid, const QString &status, const QString &validTill )
{
	this->isDigiId = true;
	this->telNumber = "";
	this->telOperator = "";
	this->docNumber = docNumber;
	this->docValid = docValid;
	this->status = status;
	this->validTill = validTill;

	updateDigiId();
}


void OtherId::updateDigiId()
{
	QString text;
	QTextStream s( &text );

	ui->lblDigiIdInfo->setText(tr("Insert the card into the reader to manage the document"));
	ui->lblIdName->setText( tr( "Digi-ID" ) );
	ui->lblLeftText->setText( docNumber );
	ui->lblRightText->setText( docValid );
	if( status == "Active" )
	{
		s << tr( "Certificates are " ) << "<span style='color: #8CC368'>"
			<< tr( "activated" ) << "</span>"
			<< tr( " and using Digi ID is " ) << "<span style='color: #8CC368'>"
			<< tr( "possible" ) << "</span>";

		ui->lblStatusText->setText( text );
		ui->lblCertText->setText( "<span style='color: #8CC368'>" + tr("Valid") + "</span> kuni " + validTill );
	}
	else
	{
		s << "<span style='color: #e80303'>"
			<< tr("Not implemented!") << "</span>";
		ui->lblStatusText->setText( text );
		ui->lblCertText->setText( "" );
	}
}

void OtherId::changeEvent(QEvent* event)
{
	if (event->type() == QEvent::LanguageChange)
	{
		ui->retranslateUi(this);

		if(isDigiId)
		{
			updateDigiId();
		}
		else
		{
			updateMobileId();
		}

	}

	QWidget::changeEvent(event);
}
