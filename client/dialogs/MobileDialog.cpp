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

#include "MobileDialog.h"
#include "ui_MobileDialog.h"

#include "Settings.h"
#include "Styles.h"
#include "effects/Overlay.h"

#include <common/IKValidator.h>

MobileDialog::MobileDialog(QWidget *parent) :
	QDialog(parent),
	ui(new Ui::MobileDialog)
{
	ui->setupUi(this);
	setWindowFlags( Qt::Dialog | Qt::FramelessWindowHint );
	setWindowModality( Qt::ApplicationModal );

	connect( ui->sign, &QPushButton::clicked, this, &MobileDialog::accept );
	connect( ui->cancel, &QPushButton::clicked, this, &MobileDialog::reject );
	connect( this, &MobileDialog::finished, this, &MobileDialog::close );

	QFont condensed12 = Styles::font( Styles::Condensed, 12 );
	QFont condensed14 = Styles::font( Styles::Condensed, 14 );
	QFont header = Styles::font( Styles::Regular, 14 );
	header.setWeight( QFont::DemiBold );
	ui->labelNameId->setFont( header );
	ui->labelPhone->setFont( condensed12 );
	ui->labelIdCode->setFont( condensed12 );
	ui->cbRemember->setFont( Styles::font( Styles::Regular, 14 ) );
	ui->sign->setFont( condensed14 );
	ui->cancel->setFont( condensed14 );

	Settings s;
	Settings s2(qApp->applicationName());
	// Mobile
	ui->idCode->setValidator( new IKValidator( ui->idCode ) );
	ui->idCode->setText( s.value( "Client/MobileCode" ).toString() );
	ui->phoneNo->setValidator( new NumberValidator( ui->phoneNo ) );
	ui->phoneNo->setText( s.value( "Client/MobileNumber" ).toString() );
	ui->cbRemember->setChecked( s2.value( "MobileSettings", true ).toBool() );
	connect(ui->idCode, &QLineEdit::textEdited, this, &MobileDialog::enableSign);
	connect(ui->phoneNo, &QLineEdit::textEdited, this, &MobileDialog::enableSign);
	connect(ui->cbRemember, &QCheckBox::clicked, [=](bool checked) {
		Settings s;
		s.setValueEx("Client/MobileCode", checked ? ui->idCode->text() : QString(), QString());
		s.setValueEx("Client/MobileNumber", checked ? ui->phoneNo->text() : QString(), QString());
		Settings(qApp->applicationName()).setValueEx("MobileSettings", checked, true);
	});

	enableSign();
}

MobileDialog::~MobileDialog()
{
	delete ui;
}

void MobileDialog::enableSign()
{
	if( ui->cbRemember->isChecked() )
	{
		Settings s;
		s.setValueEx( "Client/MobileCode", ui->idCode->text(), QString() );
		s.setValueEx( "Client/MobileNumber", ui->phoneNo->text(), QString() );
	}
	ui->sign->setToolTip(QString());
	if( !IKValidator::isValid( ui->idCode->text() ) )
		ui->sign->setToolTip( tr("Personal code is not valid") );
	if( ui->phoneNo->text().isEmpty())
		ui->sign->setToolTip( tr("Phone number is not entered") );
	ui->sign->setEnabled( ui->sign->toolTip().isEmpty() );
}

int MobileDialog::exec()
{
	Overlay overlay(parentWidget());
	overlay.show();
	auto rc = QDialog::exec();
	overlay.close();

	return rc;
}


QString MobileDialog::idCode()
{
	return ui->idCode->text();
}

QString MobileDialog::phoneNo()
{
	return ui->phoneNo->text();
}
