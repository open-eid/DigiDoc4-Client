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

#include "Styles.h"
#include "SslCertificate.h"
#include "effects/Overlay.h"
#include "dialogs/CertificateDetails.h"

KeyDialog::KeyDialog( const CKey &key, QWidget *parent )
	: QDialog( parent )
	, k( key )
	, d(new Ui::KeyDialog)
{
	d->setupUi(this);
	setWindowFlags( Qt::Dialog | Qt::CustomizeWindowHint );
	setWindowModality( Qt::ApplicationModal );
	Overlay *overlay = new Overlay(parent->topLevelWidget());
	overlay->show();
	connect(this, &KeyDialog::destroyed, overlay, &Overlay::deleteLater);

	QFont condensed = Styles::font(Styles::Condensed, 12);
	d->close->setFont(condensed);
	d->showCert->setFont(condensed);

	QStringList horzHeaders;
	horzHeaders << tr("Attribute") << tr("Value"); 
	d->view->setHeaderLabels(horzHeaders);


	connect(d->close, &QPushButton::clicked, this, &KeyDialog::accept);
	connect(d->showCert, &QPushButton::clicked, this, [&] {
		CertificateDetails::showCertificate(SslCertificate(k.cert), this);
	});

	d->title->setText( k.recipient );

	auto addItem = [&](const QString &parameter, const QString &value) {
		QTreeWidgetItem *i = new QTreeWidgetItem(d->view);
		i->setText(0, parameter);
		i->setText(1, value);
		d->view->addTopLevelItem(i);
	};

	addItem( tr("Key"), k.recipient );
	if(!k.method.isEmpty())
		addItem(tr("Crypto method"), k.method);
	if(!k.agreement.isEmpty())
		addItem(tr("Agreement method"), k.agreement);
	if(!k.derive.isEmpty())
		addItem(tr("Key derivation method"), k.derive);
	if(!k.concatDigest.isEmpty())
		addItem(tr("ConcatKDF digest method"), k.concatDigest);
	//addItem( tr("ID"), k.id );
	addItem(tr("Expires"), key.cert.expiryDate().toLocalTime().toString(QStringLiteral("dd.MM.yyyy hh:mm:ss")));
	addItem( tr("Issuer"), SslCertificate(key.cert).issuerInfo( QSslCertificate::CommonName ) );
	d->view->resizeColumnToContents( 0 );
	if(!k.agreement.isEmpty())
		adjustSize();
}

KeyDialog::~KeyDialog()
{
	delete d;
}
