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


#include "CertificateDetails.h"
#include "ui_CertificateDetails.h"
#include "effects/Overlay.h"
#include "Styles.h"

CertificateDetails::CertificateDetails(QWidget *parent) :
	QDialog(parent),
	ui(new Ui::CertificateDetails)
{
	ui->setupUi(this);
	setWindowFlags( Qt::Dialog | Qt::FramelessWindowHint );
	setWindowModality( Qt::ApplicationModal );

	QFont headerFont = Styles::font( Styles::Regular, 18 );
	QFont regularFont = Styles::font( Styles::Regular, 14 );

	ui->lblCertInfo->setFont(headerFont);
	ui->lblCertDetails->setFont(headerFont);

	ui->lblCertPurpose->setFont(regularFont);
	ui->valCertPurpose->setFont(regularFont);
	ui->lblCertIssuedTo->setFont(regularFont);
	ui->valCertIssuedTo->setFont(regularFont);
	ui->lblCertIssuer->setFont(regularFont);
	ui->valCertIssuer->setFont(regularFont);
	ui->lblCertValid->setFont(regularFont);
	ui->valCertValidFrom->setFont(regularFont);
	ui->valCertValidUntil->setFont(regularFont);

	ui->tblDetails->setFont(regularFont);


	connect( ui->close, &QPushButton::clicked, this, &CertificateDetails::accept );
	connect( this, &CertificateDetails::finished, this, &CertificateDetails::close );

	QStringList horzHeaders;
	horzHeaders << "Väli" << "Väärtus";
	ui->tblDetails->setHorizontalHeaderLabels(horzHeaders);

	ui->tblDetails->setItem(0, 0, new QTableWidgetItem("Version"));
	ui->tblDetails->setItem(0, 1, new QTableWidgetItem("V3"));
	ui->tblDetails->setItem(1, 0, new QTableWidgetItem("Seerianumber"));
	ui->tblDetails->setItem(1, 1, new QTableWidgetItem("N12 (0x321)"));
	ui->tblDetails->setItem(2, 0, new QTableWidgetItem("Signatuuri algoritm"));
	ui->tblDetails->setItem(2, 1, new QTableWidgetItem("sha1WithRSAEncryption"));
	ui->tblDetails->setItem(3, 0, new QTableWidgetItem("Välja andja"));
	ui->tblDetails->setItem(3, 1, new QTableWidgetItem("EE, SK services"));
	ui->tblDetails->setItem(4, 0, new QTableWidgetItem("Kehtib alates"));
	ui->tblDetails->setItem(4, 1, new QTableWidgetItem("01.03.2013 14:12:44 +02:00"));
	ui->tblDetails->setItem(5, 0, new QTableWidgetItem("Subjekt"));
	ui->tblDetails->setItem(5, 1, new QTableWidgetItem("01.03.2013 14:12:44 +02:00"));
	ui->tblDetails->setItem(6, 0, new QTableWidgetItem("Avalik võti"));
	ui->tblDetails->setItem(6, 1, new QTableWidgetItem("RSA (1024)"));
}

CertificateDetails::~CertificateDetails()
{
	delete ui;
}

int CertificateDetails::exec()
{
	Overlay overlay( parentWidget() );
	overlay.show();
	auto rc = QDialog::exec();
	overlay.close();

	return rc;
}
