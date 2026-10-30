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

#include "DateTime.h"
#include "SslCertificate.h"
#include "Utils.h"
#include "effects/Overlay.h"
#include "dialogs/WarningDialog.h"

#include <QtCore/QDir>
#include <QtCore/QStandardPaths>
#include <QtCore/QTextStream>
#include <QtNetwork/QSslKey>
#include <QtWidgets/QFileDialog>

#ifdef Q_OS_WIN
#include <qt_windows.h>
#include <cryptuiapi.h>
#include <wincrypt.h>
#endif

CertificateDetails::CertificateDetails(const SslCertificate &cert, QWidget *parent)
	: QDialog(parent)
{
	Ui::CertificateDetails ui;
	ui.setupUi(this);
	setAttribute(Qt::WA_DeleteOnClose);
#ifdef Q_OS_MAC
	setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint | Qt::Sheet);
#else
	setWindowFlags(Qt::Dialog | Qt::CustomizeWindowHint);
#endif
	new Overlay(this);

	const QHash<SslCertificate::EnhancedKeyUsage,QString> enhancedKeyUsageHash = cert.enhancedKeyUsage();

	QString i;
	QTextStream s( &i );
	s << "<b>" << tr("This certificate is intended for following purpose(s):") << "</b>";
	s << "<ul>";
	for(const QString &ext: enhancedKeyUsageHash)
		s << "<li>" << ext << "</li>";
	s << "</ul>";
	s << "<br />";
	s << "<b>" << tr("Issued to:") << "</b><br />" << cert.subjectInfo( QSslCertificate::CommonName).toHtmlEscaped();
	s << "<br /><br />";
	s << "<b>" << tr("Issued by:") << "</b><br />" << cert.issuerInfo(QSslCertificate::CommonName).toHtmlEscaped();
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

		if(QFile f(file); !f.open(QIODevice::WriteOnly) || f.write(cert.toPem()) < 0)
			WarningDialog::create(this)->withTitle(QCoreApplication::translate("FileDialog", "Failed to save file"))->open();
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

	auto joinInfo = [](const QList<QByteArray> &attrs, auto infoFn) {
		QStringList text, textExt;
		for(const QByteArray &obj: attrs)
		{
			QString data = infoFn(obj);
			if(data.isEmpty())
				continue;
			textExt.append(QStringLiteral("%1 = %2").arg(obj.constData(), data));
			text.append(std::move(data));
		}
		return std::pair(text.join(QStringLiteral(", ")), textExt.join('\n'));
	};

	addItem(tr("Version"), "V" + cert.version());
	addItem(tr("Serial number"), cert.serialNumber());
	addItem(tr("Signature algorithm"), cert.signatureAlgorithm());
	auto [issuerText, issuerTextExt] = joinInfo(cert.issuerInfoAttributes(),
		[&cert](const QByteArray &obj) { return cert.issuerInfo(obj); });
	addItem(tr("Issuer"), issuerText, issuerTextExt);
	addItem(tr("Valid from"), DateTime(cert.effectiveDate().toLocalTime()).toStringZ(QStringLiteral("dd.MM.yyyy hh:mm:ss")));
	addItem(tr("Valid to"), DateTime(cert.expiryDate().toLocalTime()).toStringZ(QStringLiteral("dd.MM.yyyy hh:mm:ss")));
	auto [subjectText, subjectTextExt] = joinInfo(cert.subjectInfoAttributes(),
		[&cert](const QByteArray &obj) { return cert.subjectInfo(obj); });
	addItem(tr("Subject"), subjectText, subjectTextExt);
	addItem(tr("Public key"), cert.keyName(), cert.publicKey().toDer().toHex(' ').toUpper());
	if(QStringList enhancedKeyUsage = enhancedKeyUsageHash.values(); !enhancedKeyUsage.isEmpty())
		addItem(tr("Enhanced key usage"), enhancedKeyUsage.join(QStringLiteral(", ")), enhancedKeyUsage.join('\n'));
	if(QStringList policies = cert.policies(); !policies.isEmpty())
		addItem(tr("Certificate policies"), policies.join(QStringLiteral(", ")));
	addItem(tr("Authority key identifier"), cert.authorityKeyIdentifier().toHex(' ').toUpper());
	addItem(tr("Subject key identifier"), cert.subjectKeyIdentifier().toHex(' ').toUpper());
	if(QStringList keyUsage = cert.keyUsage().values(); !keyUsage.isEmpty())
		addItem(tr("Key usage"), keyUsage.join(QStringLiteral(", ")), keyUsage.join('\n'));

	// Disable resizing
	ui.tblDetails->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
}

#ifndef Q_OS_MAC
void CertificateDetails::showCertificate(const QSslCertificate &cert, QWidget *parent, const QString &suffix)
{
#ifdef Q_OS_UNIX
	(new CertificateDetails(cert, parent))->open();
#else
	Q_UNUSED(suffix);
	QByteArray der = cert.toDer();
	if(auto ctx = make_unique_ptr<CertFreeCertificateContext>(CertCreateCertificateContext(
		X509_ASN_ENCODING, LPBYTE(der.constData()), DWORD(der.size()))))
	{
		CryptUIDlgViewContext(CERT_STORE_CERTIFICATE_CONTEXT, ctx.get(),
			parent && parent->window() ? HWND(parent->window()->winId()) : nullptr,
			nullptr, 0, nullptr);
	}
#endif
}
#endif
