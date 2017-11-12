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


#include "SignatureDialog.h"
#include "ui_SignatureDialog.h"

#include "Styles.h"
#include "dialogs/CertificateDetails.h"
#include "effects/Overlay.h"

#include <common/DateTime.h>
#include <common/SslCertificate.h>

#include <QDesktopServices>
#include <QTextStream>


#define STYLE_TERMINATOR QString("</font>")


SignatureDialog::SignatureDialog(const DigiDocSignature &signature, QWidget *parent)
:	QDialog( parent )
,	s( signature )
,	d( new Ui::SignatureDialog )
{
	d->setupUi( this );
    d->showErrors->init(false, tr("TECHNICAL INFORMATION"), d->error);
	d->showErrors->borderless();
	d->showErrors->hide();
	d->error->hide();
	
	SslCertificate c = signature.cert();
	QString style = "<font color=\"green\">";
	QString status = tr("Signature");
	if(signature.profile() == "TimeStampToken")
		status = tr("Timestamp");
	status += " ";
	switch( s.validate() )
	{
	case DigiDocSignature::Valid:
		status += tr("is valid");
		break;
	case DigiDocSignature::Warning:
		status += QString("%1%2 <font color=\"gold\">(%3)").arg(tr("is valid"), STYLE_TERMINATOR, tr("Warnings"));
		if( !s.lastError().isEmpty() )
			d->error->setPlainText( s.lastError() );
		if( s.warning() & DigiDocSignature::DigestWeak )
		{
			decorateNotice("gold");
			d->info->setText( tr(
				"The current BDOC container uses weaker encryption method than officialy accepted in Estonia.") );
		}
		break;
	case DigiDocSignature::NonQSCD:
		status += QString("%1%2 <font color=\"gold\">(%3)").arg(tr("is valid"), STYLE_TERMINATOR, tr("Restrictions"));
		decorateNotice("gold");
		d->info->setText( tr(
			"This e-Signature is not equivalent with handwritten signature and therefore "
			"can be used only in transactions where Qualified e-Signature is not required.") );
		break;
	case DigiDocSignature::Test:
		status += QString("%1 (%2)").arg(tr("is valid"), tr("Test signature"));
		if( !s.lastError().isEmpty() )
			d->error->setPlainText( s.lastError() );
		d->info->setText( tr(
			"Test signature is signed with test certificates that are similar to the "
			"certificates of real tokens, but digital signatures with legal force cannot "
			"be given with them as there is no actual owner of the card. "
			"<a href='http://www.id.ee/index.php?id=30494'>Additional information</a>.") );
		break;
	case DigiDocSignature::Invalid:
		style = "<font color=\"red\">";
		status += tr("is not valid");
		d->error->setPlainText( s.lastError().isEmpty() ? tr("Unknown error") : s.lastError() );
		decorateNotice("red");
		d->info->setText( tr(
			"This is an invalid signature or malformed digitally signed file. The signature is not valid.") );
		break;
	case DigiDocSignature::Unknown:
		style = "<font color=\"red\">";
		status += tr("is unknown");
		d->error->setPlainText( s.lastError().isEmpty() ? tr("Unknown error") : s.lastError() );
		decorateNotice("red");
		d->info->setText( tr(
			"Signature status is displayed \"unknown\" if you don't have all validity confirmation "
			"service certificates and/or certificate authority certificates installed into your computer "
			"(<a href='http://id.ee/?lang=en&id=34317'>additional information</a>).") );
		break;
	}
	bool noError = d->error->toPlainText().isEmpty();
	if(noError && d->info->text().isEmpty())
	{
		d->lblNotice->hide();
		d->info->hide();
	}
	else if(!noError)
	{
		d->showErrors->show();
		d->showErrors->borderless();		
	}

	QString name = !c.isNull() ? c.toString( c.showCN() ? "CN serialNumber" : "GN SN serialNumber" ) : s.signedBy();
	d->title->setText(QString("%1 | %2%3%4").arg(name, style, status, STYLE_TERMINATOR));
	d->close->setFont(Styles::font(Styles::Condensed, 14));
	connect(d->close, &QPushButton::clicked, this, &CertificateDetails::accept);

	const QStringList l = s.locations();
	d->signerCity->setText( l.value( 0 ) );
	d->signerState->setText( l.value( 1 ) );
	d->signerZip->setText( l.value( 2 ) );
	d->signerCountry->setText( l.value( 3 ) );

	d->signerRoles->setText( s.roles().join(", "));

	// Certificate info
	QTreeWidget *t = d->signatureView;
	t->header()->setSectionResizeMode(0, QHeaderView::ResizeToContents);

    QStringList horzHeaders;
    horzHeaders << tr("Attribute") << tr("Value");
    t->setHeaderLabels(horzHeaders);

	addItem( t, tr("Signer's Certificate issuer"), c.issuerInfo( QSslCertificate::CommonName ) );
	addItem( t, tr("Signer's Certificate"), c );
	addItem( t, tr("Signature method"), QUrl( s.signatureMethod() ) );
	addItem( t, tr("Container format"), s.parent()->mediaType() );
	if( !s.profile().isEmpty() )
		addItem( t, tr("Signature format"), s.profile() );
	if( !s.policy().isEmpty() )
	{
		#define toVer(X) (X)->toUInt() - 1
		QStringList ver = s.policy().split( "." );
		if( ver.size() >= 3 )
			addItem( t, tr("Signature policy"), QString("%1.%2.%3").arg( toVer(ver.end()-3) ).arg( toVer(ver.end()-2) ).arg( toVer(ver.end()-1) ) );
		else
			addItem( t, tr("Signature policy"), s.policy() );
	}
	addItem( t, tr("Signed file count"), QString::number( s.parent()->documentModel()->rowCount() ) );
	if( !s.spuri().isEmpty() )
		addItem( t, "SPUri", QUrl( s.spuri() ) );

	if(!s.tsaTime().isNull())
	{
		SslCertificate tsa = s.tsaCert();
		addItem( t, tr("Archive Timestamp"), DateTime( s.tsaTime().toLocalTime() ).toStringZ( "dd.MM.yyyy hh:mm:ss" ));
		addItem( t, tr("Archive Timestamp") + " (UTC)", DateTime( s.tsaTime() ).toStringZ( "dd.MM.yyyy hh:mm:ss" ) );
		addItem( t, tr("Archive TS Certificate issuer"), tsa.issuerInfo(QSslCertificate::CommonName) );
		addItem( t, tr("Archive TS Certificate"), tsa );
	}
	if(!s.tsTime().isNull())
	{
		SslCertificate ts = s.tsCert();
		addItem( t, tr("Signature Timestamp"), DateTime( s.tsTime().toLocalTime() ).toStringZ( "dd.MM.yyyy hh:mm:ss" ));
		addItem( t, tr("Signature Timestamp") + " (UTC)", DateTime( s.tsTime() ).toStringZ( "dd.MM.yyyy hh:mm:ss" ) );
		addItem( t, tr("TS Certificate issuer"), ts.issuerInfo(QSslCertificate::CommonName) );
		addItem( t, tr("TS Certificate"), ts );
	}
	if(!s.ocspTime().isNull())
	{
		SslCertificate ocsp = s.ocspCert();
		addItem( t, tr("OCSP Certificate issuer"), ocsp.issuerInfo(QSslCertificate::CommonName) );
		addItem( t, tr("OCSP Certificate"), ocsp );
		addItem( t, tr("Hash value of signature"), SslCertificate::toHex( s.ocspNonce() ) );
		addItem( t, tr("OCSP time"), DateTime( s.ocspTime().toLocalTime() ).toStringZ( "dd.MM.yyyy hh:mm:ss" ) );
		addItem( t, tr("OCSP time") + " (UTC)", DateTime( s.ocspTime() ).toStringZ( "dd.MM.yyyy hh:mm:ss" ) );
	}
	addItem( t, tr("Signer's computer time (UTC)"), DateTime( s.signTime() ).toStringZ( "dd.MM.yyyy hh:mm:ss" ) );
}

SignatureDialog::~SignatureDialog() { delete d; }

void SignatureDialog::addItem( QTreeWidget *view, const QString &variable, const QString &value )
{
	QTreeWidgetItem *i = new QTreeWidgetItem( view );
	i->setText( 0, variable );
	i->setText( 1, value );
	view->addTopLevelItem( i );
}

void SignatureDialog::addItem( QTreeWidget *view, const QString &variable, const QSslCertificate &value )
{
	QTreeWidgetItem *i = new QTreeWidgetItem( view );
	i->setText( 0, variable );
	QLabel *b = new QLabel( "<a href='cert'>" + SslCertificate(value).subjectInfo( QSslCertificate::CommonName ) + "</a>", view );
	b->setStyleSheet("margin-left: 2px");
	connect(b, &QLabel::linkActivated, [=]{ CertificateDetails( value, this ).exec(); });
	view->setItemWidget( i, 1, b );
	view->addTopLevelItem( i );
}

void SignatureDialog::addItem( QTreeWidget *view, const QString &variable, const QUrl &value )
{
	QTreeWidgetItem *i = new QTreeWidgetItem( view );
	i->setText( 0, variable );
	QLabel *b = new QLabel( "<a href='url'>" + value.toString() + "</a>", view );
	b->setStyleSheet("margin-left: 2px");
	connect(b, &QLabel::linkActivated, [=]{ QDesktopServices::openUrl( value ); });
	view->setItemWidget( i, 1, b );
	view->addTopLevelItem( i );
}

void SignatureDialog::decorateNotice(const QString color)
{
	d->lblNotice->setText(QString("<font color=\"%1\">%2</font>").arg(color, d->lblNotice->text()));
}

int SignatureDialog::exec()
{
	Overlay overlay(qApp->activeWindow());
	overlay.show();
	auto rc = QDialog::exec();
	overlay.close();

	return rc;
}
