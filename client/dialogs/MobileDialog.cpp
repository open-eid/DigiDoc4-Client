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
#include <common/Common.h>
#include "Styles.h"
#include "effects/Overlay.h"

#include <QtCore/QSettings>

#include <common/IKValidator.h>

#define COUNTRY_CODE_EST QStringLiteral("372")

MobileDialog::MobileDialog(QWidget *parent) :
	QDialog(parent),
	ui(new Ui::MobileDialog)
{
	ui->setupUi(this);
	setWindowFlags( Qt::Dialog | Qt::FramelessWindowHint );
	setWindowModality( Qt::ApplicationModal );
	setFixedSize( size() );

	connect( ui->sign, &QPushButton::clicked, this, &MobileDialog::accept );
	connect( ui->cancel, &QPushButton::clicked, this, &MobileDialog::reject );
	connect( this, &MobileDialog::finished, this, &MobileDialog::close );

	QFont condensed12 = Styles::font( Styles::Condensed, 12 );
	QFont condensed14 = Styles::font( Styles::Condensed, 14 );
	QFont header = Styles::font( Styles::Regular, 14 );
	QFont regularFont = Styles::font(Styles::Regular, 14);
	header.setWeight( QFont::DemiBold );
	ui->labelNameId->setFont( header );
	ui->labelPhone->setFont( condensed12 );
	ui->labelIdCode->setFont( condensed12 );
	ui->phoneNo->setFont(regularFont);
	ui->idCode->setFont(regularFont);
	ui->cbRemember->setFont( Styles::font( Styles::Regular, 14 ) );
	ui->sign->setFont( condensed14 );
	ui->cancel->setFont( condensed14 );

	// Mobile
	ui->idCode->setValidator( new IKValidator( ui->idCode ) );
	ui->idCode->setText(QSettings().value(QStringLiteral("MobileCode")).toString());
	ui->phoneNo->setValidator( new NumberValidator( ui->phoneNo ) );
	ui->phoneNo->setText(QSettings().value(QStringLiteral("MobileNumber"), COUNTRY_CODE_EST).toString());
	ui->cbRemember->setChecked(QSettings().value(QStringLiteral("MobileSettings"), true).toBool());
	connect(ui->idCode, &QLineEdit::textEdited, this, &MobileDialog::enableSign);
	connect(ui->phoneNo, &QLineEdit::textEdited, this, &MobileDialog::enableSign);
	connect(ui->cbRemember, &QCheckBox::clicked, this, [=](bool checked) {
		Common::setValueEx(QStringLiteral("MobileCode"), checked ? ui->idCode->text() : QString(), QString());
		Common::setValueEx(QStringLiteral("MobileNumber"), checked ? ui->phoneNo->text() : QString(), QString());
		Common::setValueEx(QStringLiteral("MobileSettings"), checked, true);
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
		Common::setValueEx(QStringLiteral("MobileCode"), ui->idCode->text(), QString());
		Common::setValueEx(QStringLiteral("MobileNumber"), ui->phoneNo->text(),
			ui->phoneNo->text() == COUNTRY_CODE_EST ? COUNTRY_CODE_EST : QString());
	}
	ui->sign->setToolTip(QString());
	if( !IKValidator::isValid( ui->idCode->text() ) )
		ui->sign->setToolTip( tr("Personal code is not valid") );
	if(ui->phoneNo->text().isEmpty() || ui->phoneNo->text() == COUNTRY_CODE_EST)
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
