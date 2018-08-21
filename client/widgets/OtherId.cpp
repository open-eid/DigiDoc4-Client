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

OtherId::OtherId(QWidget *parent)
: StyledWidget(parent)
, ui(new Ui::OtherId)
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

void OtherId::updateDigiId( const QString &docNumber, const QString &docValid, const QString &status, const QString &validTill )
{
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
	if(status == QStringLiteral("Active"))
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
		ui->lblCertText->setText(QString());
	}
}

void OtherId::changeEvent(QEvent* event)
{
	if (event->type() == QEvent::LanguageChange)
	{
		ui->retranslateUi(this);
		updateDigiId();
	}

	QWidget::changeEvent(event);
}
