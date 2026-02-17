/*
 * QDigiDocCrypto
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

#include "KeyDialog.h"
#include "ui_KeyDialog.h"

#include "CryptoDoc.h"
#include "effects/Overlay.h"
#include "dialogs/CertificateDetails.h"

KeyDialog::KeyDialog(const CDKey &k, QWidget *parent )
	: QDialog( parent )
{
	Ui::KeyDialog d;
	d.setupUi(this);
	d.showCert->hide();
#if defined (Q_OS_WIN)
	d.buttonLayout->setDirection(QBoxLayout::RightToLeft);
#endif
	setWindowFlags(Qt::Dialog|Qt::CustomizeWindowHint);
	setAttribute(Qt::WA_DeleteOnClose);
	new Overlay(this);

	connect(d.close, &QPushButton::clicked, this, &KeyDialog::accept);

	auto addItem = [view = d.view](const QString &parameter, const QString &value) {
		if(value.isEmpty())
			return;
		auto *i = new QTreeWidgetItem(view);
		i->setText(0, parameter);
		i->setText(1, value);
		view->addTopLevelItem(i);
	};

	addItem(tr("Recipient"), QString::fromStdString(k.lock.label));
	addItem(tr("Lock type"), [type = k.lock.type] {
		switch(type) {
		case libcdoc::Lock::SYMMETRIC_KEY: return QStringLiteral("SYMMETRIC_KEY");
		case libcdoc::Lock::PASSWORD: return QStringLiteral("PASSWORD");
		case libcdoc::Lock::PUBLIC_KEY: return QStringLiteral("PUBLIC_KEY");
		case libcdoc::Lock::CDOC1: return QStringLiteral("CDOC1");
		case libcdoc::Lock::SERVER: return QStringLiteral("SERVER");
		case libcdoc::Lock::SHARE_SERVER: return QStringLiteral("SHARE_SERVER");
		default: return QStringLiteral("INVALID");
		}
	}());
	QSslCertificate cert;
	if (!k.rcpt_cert.isNull()) {
		cert = k.rcpt_cert;
	} else if (k.lock.isCDoc1()) {
		const std::vector<uint8_t> &certData = k.lock.getBytes(libcdoc::Lock::Params::CERT);
		cert = QSslCertificate(QByteArray::fromRawData(reinterpret_cast<const char *>(certData.data()), certData.size()), QSsl::Der);
	}
	if (!cert.isNull()) {
		connect(d.showCert, &QPushButton::clicked, this, [this, cert] {
			CertificateDetails::showCertificate(cert, this);
		});
		d.showCert->show();
		if (k.lock.isCDoc1()) {
			std::string cdigest = k.lock.getString(libcdoc::Lock::Params::CONCAT_DIGEST);
			addItem(tr("ConcatKDF digest method"), QString::fromStdString(cdigest));
		}
		addItem(tr("Expiry date"), cert.expiryDate().toLocalTime().toString(QStringLiteral("dd.MM.yyyy hh:mm:ss")));
		addItem(tr("Issuer"), cert.issuerInfo(QSslCertificate::CommonName).join(" "));
		d.view->resizeColumnToContents(0);
	} else if (k.lock.type == libcdoc::Lock::SERVER) {
		addItem(tr("Key server ID"), QString::fromUtf8(k.lock.getString(libcdoc::Lock::Params::KEYSERVER_ID)));
		addItem(tr("Transaction ID"), QString::fromUtf8(k.lock.getString(libcdoc::Lock::Params::TRANSACTION_ID)));
	}
	d.view->resizeColumnToContents( 0 );
	adjustSize();
}
