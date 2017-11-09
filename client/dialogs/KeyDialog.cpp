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
#include "dialogs/CertificateDetails.h"
#include "effects/Overlay.h"

#include <common/SslCertificate.h>



KeyDialog::KeyDialog( const CKey &key, QWidget *parent )
:	QDialog( parent )
,	k( key )
,	d(new Ui::KeyDialog)
{
	d->setupUi(this);
	setWindowFlags( Qt::Dialog | Qt::FramelessWindowHint );
	setWindowModality( Qt::ApplicationModal );

	QFont condensed = Styles::font(Styles::Condensed, 12);
	d->close->setFont(condensed);
	d->showCert->setFont(condensed);

	QStringList horzHeaders;
	horzHeaders << tr("Attribute") << tr("Value");
	d->view->setHeaderLabels(horzHeaders);


	connect(d->close, &QPushButton::clicked, this, &KeyDialog::accept);
	connect(d->showCert, &QPushButton::clicked, this, &KeyDialog::showCertificate);

	d->title->setText( k.recipient );

	addItem( tr("Key"), k.recipient );
	addItem( tr("Crypto method"), k.method );
	//addItem( tr("ID"), k.id );
	addItem( tr("Expires"), key.cert.expiryDate().toLocalTime().toString("dd.MM.yyyy hh:mm:ss") );
	addItem( tr("Issuer"), SslCertificate(key.cert).issuerInfo( QSslCertificate::CommonName ) );
	d->view->resizeColumnToContents( 0 );
#ifdef Q_OS_WIN32
	setStyleSheet("QWidget#KeyDialog{ border-radius: 2px; background-color: #FFFFFF; border: solid #D9D9D8; border-width: 1px 1px 1px 1px;}");
#endif
}

KeyDialog::~KeyDialog()
{
	delete d;
}

void KeyDialog::addItem( const QString &parameter, const QString &value )
{
	QTreeWidgetItem *i = new QTreeWidgetItem( d->view );
	i->setText( 0, parameter );
	i->setText( 1, value );
	d->view->addTopLevelItem( i );
}

void KeyDialog::showCertificate()
{
	CertificateDetails dlg(k.cert, this);
	dlg.exec();
}

int KeyDialog::exec()
{
	Overlay overlay(parentWidget());
	overlay.show();
	auto rc = QDialog::exec();
	overlay.close();

	return rc;
}
