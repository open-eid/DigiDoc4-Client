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
	StyledWidget( parent ),
	ui( new Ui::OtherId )
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

	// top | right | bottom | left
	this->setStyleSheet( "background-color: #ffffff; border: solid #DFE5E9; border-width: 1px 0px 0px 1px;" );
}

OtherId::~OtherId()
{
	delete ui;
}

void OtherId::update( const QString &lblIdName )
{
	ui->lblIdName->setText( lblIdName );
}

void OtherId::updateMobileId( const QString &telNumber, const QString &telOperator, const QString &status, const QString &validTill )
{
	QString text;
	QTextStream s( &text );

    ui->lblDigiIdInfo->setText( "" );
	ui->lblIdName->setText( tr( "Mobiil-ID" ) );
	ui->lblLeftHdr->setText( tr( "TELEFONI NUMBER" ) );
	ui->lblLeftText->setText( "+" + telNumber );
 	ui->lblRightHdr->setText( tr( "MOBIILI OPERAATOR" ) );
	ui->lblRightText->setText( telOperator );
	if( status == "Active" )
	{
		s << tr( "Sertifikaadid on " ) << "<span style='color: #8CC368'>"
			<< tr( "aktiivsed" ) << "</span>"
			<< tr( " ja Mobiil-ID kasutamine on " ) << "<span style='color: #8CC368'>"
			<< tr( "võimalik" ) << "</span>";

		ui->lblStatusText->setText( text );
		ui->lblCertText->setText( "<span style='color: #8CC368'>Kehtivad</span> kuni " + validTill );
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
	QString text;
	QTextStream s( &text );

	ui->lblIdName->setText( tr( "Digi-ID" ) );
	ui->lblLeftText->setText( docNumber );
	ui->lblRightText->setText( docValid );
	if( status == "Active" )
	{
		s << tr( "Sertifikaadid on " ) << "<span style='color: #8CC368'>"
			<< tr( "aktiivsed" ) << "</span>"
			<< tr( " ja Digi-ID kasutamine on " ) << "<span style='color: #8CC368'>"
			<< tr( "võimalik" ) << "</span>";

		ui->lblStatusText->setText( text );
		ui->lblCertText->setText( "<span style='color: #8CC368'>Kehtivad</span> kuni " + validTill );
	}
	else
	{
		s << "<span style='color: #e80303'>"
			<< "Not implemented!" << "</span>";
		ui->lblStatusText->setText( text );
		ui->lblCertText->setText( "" );
	}
}
