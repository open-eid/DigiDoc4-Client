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
#include "Styles.h"
#include "SslCertificate.h"
#include "effects/Overlay.h"
#include "dialogs/CertificateDetails.h"

#include <memory>

KeyDialog::KeyDialog( const CKey &k, QWidget *parent )
	: QDialog( parent )
{
	auto d = std::make_unique<Ui::KeyDialog>();
	d->setupUi(this);
#if defined (Q_OS_WIN)
	d->buttonLayout->setDirection(QBoxLayout::RightToLeft);
#endif
	setWindowFlags(Qt::Dialog|Qt::CustomizeWindowHint);
	setAttribute(Qt::WA_DeleteOnClose);
	new Overlay(this);

	QFont condensed = Styles::font(Styles::Condensed, 12);
	QFont regular = Styles::font(Styles::Regular, 14);
	d->close->setFont(condensed);
	d->showCert->setFont(condensed);
	d->view->header()->setFont(regular);
	d->view->setFont(regular);
	d->view->setHeaderLabels({tr("Attribute"), tr("Value")});

	connect(d->close, &QPushButton::clicked, this, &KeyDialog::accept);
	if (k.type == CKey::CDOC1) {
		const CKeyCD1& kd = static_cast<const CKeyCD1&>(k);
		connect(d->showCert, &QPushButton::clicked, this, [this, cert=kd.cert] {
			CertificateDetails::showCertificate(cert, this);
		});
		d->showCert->setHidden(kd.cert.isNull());
	} else {
		d->showCert->setHidden(true);
	}

	auto addItem = [&](const QString &parameter, const QString &value) {
		if(value.isEmpty())
			return;
		auto *i = new QTreeWidgetItem(d->view);
		i->setText(0, parameter);
		i->setText(1, value);
		d->view->addTopLevelItem(i);
	};

	bool adjust_size = false;
	if (k.type == CKey::Type::CDOC1) {
		const CKeyCD1& cd1key = static_cast<const CKeyCD1&>(k);
		addItem(tr("Recipient"), cd1key.recipient);
		addItem(tr("Crypto method"), cd1key.method);
		addItem(tr("Agreement method"), cd1key.agreement);
		addItem(tr("Key derivation method"), cd1key.derive);
		addItem(tr("ConcatKDF digest method"), cd1key.concatDigest);
		addItem(tr("Expiry date"), cd1key.cert.expiryDate().toLocalTime().toString(QStringLiteral("dd.MM.yyyy hh:mm:ss")));
		addItem(tr("Issuer"), SslCertificate(cd1key.cert).issuerInfo(QSslCertificate::CommonName));
		adjust_size = !cd1key.agreement.isEmpty();
	} else if (CKeyCD2::isCDoc2Key(k)) {
		const CKeyCD2& cd2key = static_cast<const CKeyCD2&>(k);
		addItem(tr("Label"), cd2key.label);
		if (k.type == CKey::SERVER) {
			const CKeyServer& sk = static_cast<const CKeyServer&>(k);
			addItem(tr("Key server ID"), sk.keyserver_id);
			addItem(tr("Transaction ID"), sk.transaction_id);
		}
	}
	d->view->resizeColumnToContents( 0 );
	if(adjust_size) adjustSize();
}
