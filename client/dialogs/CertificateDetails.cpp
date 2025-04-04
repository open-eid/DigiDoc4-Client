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

#include "Application.h"
#include "DateTime.h"
#include "SslCertificate.h"
#include "effects/Overlay.h"
#include "dialogs/WarningDialog.h"

#include <QtCore/QDir>
#include <QtCore/QStandardPaths>
#include <QtCore/QTextStream>
#include <QtGui/QDesktopServices>
#include <QtNetwork/QSslKey>
#include <QtWidgets/QFileDialog>

CertificateDetails::CertificateDetails(const SslCertificate &cert, QWidget *parent)
	: QDialog(parent)
{
	Ui::CertificateDetails ui;
	ui.setupUi(this);
#ifdef Q_OS_MAC
	setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint | Qt::Sheet);
#else
	setWindowFlags(Qt::Dialog | Qt::CustomizeWindowHint);
#endif
	new Overlay(this);

	QString i;
	QTextStream s( &i );
	s << "<b>" << tr("This certificate is intended for following purpose(s):") << "</b>";
	s << "<ul>";
	for(const QString &ext: cert.enhancedKeyUsage())
		s << "<li>" << tr(ext.toStdString().c_str()) << "</li>";
	s << "</ul>";
	s << "<br />";
	s << "<b>" << tr("Issued to:") << "</b><br />" << cert.subjectInfo( QSslCertificate::CommonName);
	s << "<br /><br />";
	s << "<b>" << tr("Issued by:") << "</b><br />" << cert.issuerInfo(QSslCertificate::CommonName);
	s << "<br /><br />";
	s << "<b>" << tr("Valid:") << "</b><br />";
	s << "<b>" << tr("From") << "</b> " << cert.effectiveDate().toLocalTime().toString(QStringLiteral("dd.MM.yyyy")) << "<br />";
	s << "<b>" << tr("To") << "</b> " << cert.expiryDate().toLocalTime().toString(QStringLiteral("dd.MM.yyyy"));
	ui.lblCertInfo->setHtml( i );

	connect(ui.save, &QPushButton::clicked, this, [this, cert] {
		QString file = QFileDialog::getSaveFileName(this, tr("Save certificate"), QStringLiteral("%1%2%3.cer")
				.arg(QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation))
				.arg(QDir::separator())
				.arg(cert.subjectInfo("serialNumber")),
			tr("Certificates (*.cer *.crt *.pem)"));
		if( file.isEmpty() )
			return;

		if(QFile f(file); f.open(QIODevice::WriteOnly))
			f.write(cert.toPem());
		else
			WarningDialog::show(this, tr("Failed to save file"));
	});
	connect(ui.close, &QPushButton::clicked, this, &CertificateDetails::accept);
	connect( this, &CertificateDetails::finished, this, &CertificateDetails::close );
	connect(ui.tblDetails, &QTableWidget::itemSelectionChanged, this, [detailedValue = ui.detailedValue, tblDetails = ui.tblDetails] {
		const QList<QTableWidgetItem*> &list = tblDetails->selectedItems();
		if(list.isEmpty())
			return;
		auto *contentItem = list.last();
		auto userData = contentItem->data(Qt::UserRole);
		detailedValue->setPlainText(userData.isNull() ?
			contentItem->data(Qt::DisplayRole).toString() : userData.toString());
	});

	auto addItem = [tblDetails = ui.tblDetails](const QString &variable, const QString &value, const QVariant &valueext = {}) {
		int row = tblDetails->model()->rowCount();
		tblDetails->setRowCount(row + 1);
		auto *item = new QTableWidgetItem(value);
		item->setData(Qt::UserRole, valueext);
		tblDetails->setItem(row, 0, new QTableWidgetItem(variable));
		tblDetails->setItem(row, 1, item);
	};

	addItem(tr("Version"), QString("V" + cert.version()));
	addItem(tr("Serial number"), cert.serialNumber());
	addItem(tr("Signature algorithm"), cert.signatureAlgorithm());

	QStringList text, textExt;
	static const QByteArray ORGID_OID = QByteArrayLiteral("2.5.4.97");
	for(const QByteArray &obj: cert.issuerInfoAttributes())
	{
		const QString &data = cert.issuerInfo( obj );
		if( data.isEmpty() )
			continue;
		text << data;
		// organizationIdentifier OID might not be known by SSL backend
		textExt << QStringLiteral("%1 = %2").arg(
			obj.constData() == ORGID_OID ? "organizationIdentifier" : obj.constData(), data);
	}
	addItem(tr("Issuer"), text.join(QStringLiteral(", ")), textExt.join('\n'));
	addItem(tr("Valid from"), DateTime(cert.effectiveDate().toLocalTime()).toStringZ(QStringLiteral("dd.MM.yyyy hh:mm:ss")));
	addItem(tr("Valid to"), DateTime(cert.expiryDate().toLocalTime()).toStringZ(QStringLiteral("dd.MM.yyyy hh:mm:ss")));

	text.clear();
	textExt.clear();
	for(const QByteArray &obj: cert.subjectInfoAttributes())
	{
		const QString &data = cert.subjectInfo( obj );
		if( data.isEmpty() )
			continue;
		text << data;
		textExt << QStringLiteral("%1 = %2").arg(obj.constData(), data);
	}
	addItem(tr("Subject"), text.join(QStringLiteral(", ")), textExt.join('\n'));
	addItem(tr("Public key"), cert.keyName(), cert.publicKey().toDer().toHex(' ').toUpper());
	QStringList enhancedKeyUsage = cert.enhancedKeyUsage().values();
	if( !enhancedKeyUsage.isEmpty() )
		addItem(tr("Enhanced key usage"), enhancedKeyUsage.join(QStringLiteral(", ")), enhancedKeyUsage.join('\n'));
	QStringList policies = cert.policies();
	if( !policies.isEmpty() )
		addItem(tr("Certificate policies"), policies.join(QStringLiteral(", ")));
	addItem(tr("Authority key identifier"), cert.authorityKeyIdentifier().toHex(' ').toUpper());
	addItem(tr("Subject key identifier"), cert.subjectKeyIdentifier().toHex(' ').toUpper());
	QStringList keyUsage = cert.keyUsage().values();
	if( !keyUsage.isEmpty() )
		addItem(tr("Key usage"), keyUsage.join(QStringLiteral(", ")), keyUsage.join('\n'));

	// Disable resizing
	ui.tblDetails->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
}

#ifndef Q_OS_MAC
void CertificateDetails::showCertificate(const QSslCertificate &cert, QWidget *parent, const QString &suffix)
{
#ifdef Q_OS_UNIX
	CertificateDetails(cert, parent).exec();
#else
	Q_UNUSED(parent);
	QString name = cert.subjectInfo("serialNumber").join('_');
	if(name.isEmpty())
		name = cert.serialNumber().replace(':', "");
	QString path = QStringLiteral("%1/%2%3.cer").arg(QDir::tempPath(), name, suffix);
	if(QFile f(path); f.open(QIODevice::WriteOnly))
		f.write(cert.toPem());
	qApp->addTempFile(path);
	QDesktopServices::openUrl(QUrl::fromLocalFile(path));
#endif
}
#endif
