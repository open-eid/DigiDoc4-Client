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
#include "Styles.h"
#include "effects/Overlay.h"

MobileDialog::MobileDialog(QWidget *parent) :
	QDialog(parent),
	ui(new Ui::MobileDialog)
{
	ui->setupUi(this);
	setWindowFlags( Qt::Dialog | Qt::FramelessWindowHint );
	setWindowModality( Qt::ApplicationModal );

	connect( ui->ok, &QPushButton::clicked, this, &MobileDialog::accept );
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
	ui->ok->setFont( condensed14 );
	ui->cancel->setFont( condensed14 );
}

MobileDialog::~MobileDialog()
{
	delete ui;
}

int MobileDialog::exec()
{
	Overlay overlay(parentWidget());
	overlay.show();
	auto rc = QDialog::exec();
	overlay.close();

	return rc;
}