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

#include "Styles.h"
#include "effects/Overlay.h"

#include <common/DateTime.h>
#include <common/SslCertificate.h>

#include <QTextStream>

CertificateDetails::CertificateDetails(const QSslCertificate &cert, QWidget *parent) :
	QDialog(parent),
	ui(new Ui::CertificateDetails)
{
	ui->setupUi(this);
	setWindowFlags( Qt::Dialog | Qt::FramelessWindowHint );
	setWindowModality( Qt::ApplicationModal );

	QFont headerFont = Styles::font( Styles::Regular, 18 );
	QFont regularFont = Styles::font( Styles::Regular, 14 );
	QFont smallFont = Styles::font( Styles::Regular, 12 );

	ui->lblCertHeader->setFont(headerFont);
	ui->lblCertDetails->setFont(headerFont);

	ui->lblCertInfo->setFont(regularFont);
	ui->tblDetails->setFont(smallFont);

	ui->lblCertHeader->setText(tr("Certificate Information"));

	SslCertificate c = cert;
	QString i;
	QTextStream s( &i );
	s << "<b>" << tr("This certificate is intended for following purpose(s):") << "</b>";
	s << "<ul>";
	for(const QString &ext: c.enhancedKeyUsage())
		s << "<li>" << tr(ext.toStdString().c_str()) << "</li>";
	s << "</ul>";
	s << "<br />";
	s << "<b>" << tr("Issued to:") << "</b><br />" << c.subjectInfo( QSslCertificate::CommonName );
	s << "<br /><br />";
	s << "<b>" << tr("Issued by:") << "</b><br />" << c.issuerInfo( QSslCertificate::CommonName );
	s << "<br /><br />";
	s << "<b>" << tr("Valid:") << "</b><br />";
	s << "<b>" << tr("From") << "</b> " << c.effectiveDate().toLocalTime().toString( "dd.MM.yyyy" ) << "<br />";
	s << "<b>" << tr("To") << "</b> " << c.expiryDate().toLocalTime().toString( "dd.MM.yyyy" );
	ui->lblCertInfo->setHtml( i );

	connect( ui->close, &QPushButton::clicked, this, &CertificateDetails::accept );
	connect( this, &CertificateDetails::finished, this, &CertificateDetails::close );

	QStringList horzHeaders;
	horzHeaders << "Väli" << "Väärtus";
	ui->tblDetails->setHorizontalHeaderLabels(horzHeaders);

	ui->tblDetails->setItem(0, 0, new QTableWidgetItem("Version"));
	ui->tblDetails->setItem(0, 1, new QTableWidgetItem(QString("V" + c.version())));
	ui->tblDetails->setItem(1, 0, new QTableWidgetItem("Seerianumber"));
	ui->tblDetails->setItem(1, 1, new QTableWidgetItem(QString( "%1 (0x%2)" )
		.arg( c.serialNumber().constData() )
		.arg( c.serialNumber( true ).constData() )));
	ui->tblDetails->setItem(2, 0, new QTableWidgetItem("Signatuuri algoritm"));
	ui->tblDetails->setItem(2, 1, new QTableWidgetItem(c.signatureAlgorithm()));
	ui->tblDetails->setItem(3, 0, new QTableWidgetItem("Väljaandja"));

	QStringList text, textExt;
	static const QByteArray ORGID_OID = QByteArrayLiteral("2.5.4.97");
	for(const QByteArray &obj: c.issuerInfoAttributes())
	{
		const QString &data = c.issuerInfo( obj );
		if( data.isEmpty() )
			continue;
		text << data;
		// organizationIdentifier OID might not be known by SSL backend
		textExt << QString( "%1 = %2" ).arg(
				obj.constData() == ORGID_OID ? "organizationIdentifier" : obj.constData()
			).arg( data );
	}
	QTableWidgetItem *item = new QTableWidgetItem(text.join(", "));
	item->setData(Qt::UserRole, textExt.join("\n"));
	ui->tblDetails->setItem(3, 1, item);
	ui->tblDetails->setItem(4, 0, new QTableWidgetItem("Kehtib alates"));
	ui->tblDetails->setItem(4, 1, new QTableWidgetItem(DateTime( c.effectiveDate().toLocalTime() ).toStringZ("dd.MM.yyyy hh:mm:ss")));
	ui->tblDetails->setItem(5, 0, new QTableWidgetItem("Kehtib kuni"));
	ui->tblDetails->setItem(5, 1, new QTableWidgetItem(DateTime( c. expiryDate().toLocalTime() ).toStringZ("dd.MM.yyyy hh:mm:ss")));

	text.clear();
	textExt.clear();
	for(const QByteArray &obj: c.subjectInfoAttributes())
	{
		const QString &data = c.subjectInfo( obj );
		if( data.isEmpty() )
			continue;
		text << data;
		textExt << QString( "%1 = %2" ).arg( obj.constData() ).arg( data );
	}
	ui->tblDetails->setItem(6, 0, new QTableWidgetItem("Subjekt"));
	ui->tblDetails->setItem(6, 1, new QTableWidgetItem(text.join( ", " )));
	ui->tblDetails->setItem(7, 0, new QTableWidgetItem("Avalik võti"));
	ui->tblDetails->setItem(7, 1, new QTableWidgetItem(c.keyName()));
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
