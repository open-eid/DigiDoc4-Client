// SPDX-FileCopyrightText: Estonian Information System Authority
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "KeyDialog.h"
#include "ui_KeyDialog.h"

#include "CryptoDoc.h"
#include "effects/Overlay.h"
#include "dialogs/CertificateDetails.h"

KeyDialog::KeyDialog( const CKey &k, QWidget *parent )
	: QDialog( parent )
{
	Ui::KeyDialog d;
	d.setupUi(this);
#if defined (Q_OS_WIN)
	d.buttonLayout->setDirection(QBoxLayout::RightToLeft);
#endif
	setWindowFlags(Qt::Dialog|Qt::CustomizeWindowHint);
	setAttribute(Qt::WA_DeleteOnClose);
	new Overlay(this);

	connect(d.close, &QPushButton::clicked, this, &KeyDialog::accept);
	connect(d.showCert, &QPushButton::clicked, this, [this, cert=k.cert] {
		CertificateDetails::showCertificate(cert, this);
	});
	d.showCert->setHidden(k.cert.isNull());

	auto addItem = [view = d.view](const QString &parameter, const QString &value) {
		if(value.isEmpty())
			return;
		auto *i = new QTreeWidgetItem(view);
		i->setText(0, parameter);
		i->setText(1, value);
		view->addTopLevelItem(i);
	};

	addItem(tr("Recipient"), k.recipient);
	addItem(tr("ConcatKDF digest method"), k.concatDigest);
	addItem(tr("Key server ID"), k.keyserver_id);
	addItem(tr("Transaction ID"), k.transaction_id);
	addItem(tr("Expiry date"), k.cert.expiryDate().toLocalTime().toString(QStringLiteral("dd.MM.yyyy hh:mm:ss")));
	addItem(tr("Issuer"), k.cert.issuerInfo(QSslCertificate::CommonName).join(' '));
	d.view->resizeColumnToContents( 0 );
	adjustSize();
}
