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

#include <QDir>
#include <QFileDialog>
#include <QMessageBox>
#include <QStandardPaths>
#include <QTextStream>


class CertificateDetailsPrivate: public Ui::CertificateDetails
{
public:
	void addItem( const QString &variable, const QString &value, const QVariant &valueext = QVariant() )
	{
		int row = tblDetails->model()->rowCount();
		tblDetails->setRowCount(row + 1);
		QTableWidgetItem *item = new QTableWidgetItem(value);
		item->setData(Qt::UserRole, valueext);
		tblDetails->setItem(row, 0, new QTableWidgetItem(variable));
		tblDetails->setItem(row, 1, item);
	}
};



CertificateDetails::CertificateDetails(const QSslCertificate &qSslCert, QWidget *parent) :
	QDialog(parent),
	ui(new CertificateDetailsPrivate),
	cert(qSslCert)
{
	ui->setupUi(this);
	setWindowFlags( Qt::Dialog | Qt::CustomizeWindowHint );
	setWindowModality( Qt::ApplicationModal );

	QFont headerFont = Styles::font( Styles::Regular, 18 );
	QFont regularFont = Styles::font( Styles::Regular, 14 );
	QFont smallFont = Styles::font(Styles::Regular, 13);

	ui->lblCertHeader->setFont(headerFont);
	ui->lblCertDetails->setFont(headerFont);

	ui->lblCertInfo->setFont(regularFont);
	ui->tblDetails->setFont(smallFont);
	ui->detailedValue->setFont(smallFont);

	QFont condensed14 = Styles::font(Styles::Condensed, 14);
	ui->close->setFont(condensed14);
	ui->save->setFont(condensed14);

	ui->lblCertHeader->setText(tr("Certificate information"));

	QString i;
	QTextStream s( &i );
	s << "<b>" << tr("This certificate is intended for following purpose(s):") << "</b>";
	s << "<ul>";
	for(const QString &ext: cert.enhancedKeyUsage())
		s << "<li>" << tr(ext.toStdString().c_str()) << "</li>";
	s << "</ul>";
	s << "<br />";
	s << "<b>" << tr("Issued to:") << "</b><br />" << cert.subjectInfo( QSslCertificate::CommonName );
	s << "<br /><br />";
	s << "<b>" << tr("Issued by:") << "</b><br />" << cert.issuerInfo( QSslCertificate::CommonName );
	s << "<br /><br />";
	s << "<b>" << tr("Valid:") << "</b><br />";
	s << "<b>" << tr("From") << "</b> " << cert.effectiveDate().toLocalTime().toString( "dd.MM.yyyy" ) << "<br />";
	s << "<b>" << tr("To") << "</b> " << cert.expiryDate().toLocalTime().toString( "dd.MM.yyyy" );
	ui->lblCertInfo->setHtml( i );


	connect( ui->save, &QPushButton::clicked, this, &CertificateDetails::saveCert );
	connect( ui->close, &QPushButton::clicked, this, &CertificateDetails::accept );
	connect( this, &CertificateDetails::finished, this, &CertificateDetails::close );

	QStringList horzHeaders;
	horzHeaders << tr("Field") << tr("Value");
	ui->tblDetails->setHorizontalHeaderLabels(horzHeaders);

	ui->addItem(tr("Version"), QString("V" + cert.version()));
	ui->addItem(tr("Serial number"), QString( "%1 (0x%2)" )
		.arg( cert.serialNumber().constData() )
		.arg( cert.serialNumber( true ).constData() ));
	ui->addItem(tr("Signature algorithm"), cert.signatureAlgorithm());

	QStringList text, textExt;
	static const QByteArray ORGID_OID = QByteArrayLiteral("2.5.4.97");
	for(const QByteArray &obj: cert.issuerInfoAttributes())
	{
		const QString &data = cert.issuerInfo( obj );
		if( data.isEmpty() )
			continue;
		text << data;
		// organizationIdentifier OID might not be known by SSL backend
		textExt << QString( "%1 = %2" ).arg(
				obj.constData() == ORGID_OID ? "organizationIdentifier" : obj.constData()
			).arg( data );
	}
	ui->addItem(tr("Issuer"), text.join(", "), textExt.join("\n"));
	ui->addItem(tr("Valid from"), DateTime( cert.effectiveDate().toLocalTime() ).toStringZ("dd.MM.yyyy hh:mm:ss"));
	ui->addItem(tr("Valid to"), DateTime( cert.expiryDate().toLocalTime() ).toStringZ("dd.MM.yyyy hh:mm:ss"));

	text.clear();
	textExt.clear();
	for(const QByteArray &obj: cert.subjectInfoAttributes())
	{
		const QString &data = cert.subjectInfo( obj );
		if( data.isEmpty() )
			continue;
		text << data;
		textExt << QString( "%1 = %2" ).arg( obj.constData() ).arg( data );
	}
	ui->addItem(tr("Subject"), text.join(", "), textExt.join("\n"));
	ui->addItem(tr("Public key"), cert.keyName(), cert.publicKeyHex());
	QStringList enhancedKeyUsage = cert.enhancedKeyUsage().values();
	if( !enhancedKeyUsage.isEmpty() )
		ui->addItem(tr("Enhanced key usage"), enhancedKeyUsage.join( ", " ), enhancedKeyUsage.join( "\n" ) );
	QStringList policies = cert.policies();
	if( !policies.isEmpty() )
		ui->addItem( tr("Certificate policies"), policies.join( ", " ) );
	ui->addItem( tr("Authority key identifier"), cert.toHex( cert.authorityKeyIdentifier() ) );
	ui->addItem( tr("Subject key identifier"), cert.toHex( cert.subjectKeyIdentifier() ) );
	QStringList keyUsage = cert.keyUsage().values();
	if( !keyUsage.isEmpty() )
		ui->addItem( tr("Key usage"), keyUsage.join( ", " ), keyUsage.join( "\n" ) );

	// Disable resizing
	ui->tblDetails->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
}

CertificateDetails::~CertificateDetails()
{
	delete ui;
}

void CertificateDetails::saveCert()
{
	QString file = QFileDialog::getSaveFileName(this, tr("Save certificate"), QString("%1%2%3.cer")
			.arg(QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation))
			.arg(QDir::separator())
			.arg(cert.subjectInfo("serialNumber")),
		tr("Certificates (*.cer *.crt *.pem)"));
	if( file.isEmpty() )
		return;

	QFile f( file );
	if( f.open( QIODevice::WriteOnly ) )
		f.write( cert.toPem() );
	else
		QMessageBox::warning( this, tr("Save certificate"), tr("Failed to save file") );
}

int CertificateDetails::exec()
{
	Overlay overlay( parentWidget() );
	overlay.show();
	auto rc = QDialog::exec();
	overlay.close();

	return rc;
}

void CertificateDetails::on_tblDetails_itemSelectionChanged()
{
	const QList<QTableWidgetItem*> &list = ui->tblDetails->selectedItems();
	if( !list.isEmpty() )
	{
		auto contentItem = list.last(); //constLast();
		auto userData = contentItem->data(Qt::UserRole);
		ui->detailedValue->setPlainText(userData.isNull() ?
			contentItem->data(Qt::DisplayRole).toString() : userData.toString());
	}
}
