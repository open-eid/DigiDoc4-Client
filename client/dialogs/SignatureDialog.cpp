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
#include "SslCertificate.h"
#include "dialogs/CertificateDetails.h"
#include "effects/Overlay.h"

#include <common/DateTime.h>

#include <QDesktopServices>

SignatureDialog::SignatureDialog(const DigiDocSignature &signature, QWidget *parent)
:	QDialog( parent )
,	s( signature )
,	d( new Ui::SignatureDialog )
{
	d->setupUi( this );
	setAttribute(Qt::WA_DeleteOnClose);
	setWindowFlags(windowFlags()|Qt::FramelessWindowHint);
	d->showErrors->init(false, tr("TECHNICAL INFORMATION"), tr("Technical information", "accessible"), d->error);
	d->showErrors->borderless();
	d->showErrors->setClosable(true);
	d->showErrors->hide();
	d->error->hide();

	Overlay *overlay = new Overlay(parent->topLevelWidget());
	overlay->show();
	connect(this, &SignatureDialog::destroyed, overlay, &Overlay::deleteLater);

	SslCertificate c = signature.cert();
	QString style = QStringLiteral("<font color=\"green\">");
	QString status = tr("Signature");
	bool isTS = signature.profile() == QStringLiteral("TimeStampToken");
	if(isTS)
		status = tr("Timestamp");
	status += ' ';
	auto isValid = [](bool isTS) {
		return isTS ? tr("is valid", "Timestamp") : tr("is valid", "Signature");
	};
	switch( s.validate() )
	{
	case DigiDocSignature::Valid:
		status += isValid(isTS);
		break;
	case DigiDocSignature::Warning:
		status += QStringLiteral("%1</font> <font color=\"gold\">(%2)").arg(isValid(isTS), tr("Warnings"));
		decorateNotice(QStringLiteral("gold"));
		if(!s.lastError().isEmpty())
			d->error->setPlainText( s.lastError() );
		if(s.warning() & DigiDocSignature::DigestWeak )
			d->info->setText( tr(
				"The current BDOC container uses weaker encryption method than officialy accepted in Estonia.") );
		else
			d->info->setText(tr("SIGNATURE_WARNING"));
		break;
	case DigiDocSignature::NonQSCD:
		status += QStringLiteral("%1</font> <font color=\"gold\">(%2)").arg(isValid(isTS), tr("Restrictions"));
		decorateNotice(QStringLiteral("gold"));
		d->info->setText( tr(
			"This e-Signature is not equivalent with handwritten signature and therefore "
			"can be used only in transactions where Qualified e-Signature is not required.") );
		break;
	case DigiDocSignature::Test:
		status += QStringLiteral("%1 (%2)").arg(isValid(isTS), tr("Test signature"));
		if( !s.lastError().isEmpty() )
			d->error->setPlainText( s.lastError() );
		d->info->setText( tr(
			"Test signature is signed with test certificates that are similar to the "
			"certificates of real tokens, but digital signatures with legal force cannot "
			"be given with them as there is no actual owner of the card. "
			"<a href='http://www.id.ee/index.php?id=30494'>Additional information</a>.") );
		break;
	case DigiDocSignature::Invalid:
		style = QStringLiteral("<font color=\"red\">");
		status += isTS ? tr("is not valid", "Timestamp") :  tr("is not valid", "Signature");
		d->error->setPlainText( s.lastError().isEmpty() ? tr("Unknown error") : s.lastError() );
		decorateNotice(QStringLiteral("red"));
		d->info->setText( tr(
			"This is an invalid signature or malformed digitally signed file. The signature is not valid.") );
		break;
	case DigiDocSignature::Unknown:
		style = QStringLiteral("<font color=\"red\">");
		status += isTS ? tr("is unknown", "Timestamp") : tr("is unknown", "Signature");
		d->error->setPlainText( s.lastError().isEmpty() ? tr("Unknown error") : s.lastError() );
		decorateNotice(QStringLiteral("red"));
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

	QString name = !c.isNull() ? c.toString(c.showCN() ? QStringLiteral("CN serialNumber") : QStringLiteral("GN SN serialNumber")) : s.signedBy();
	d->title->setText(QStringLiteral("%1 | %2%3</font>").arg(name, style, status));
	d->close->setFont(Styles::font(Styles::Condensed, 14));
	connect(d->close, &QPushButton::clicked, this, &SignatureDialog::accept);

	QFont header = Styles::font(Styles::Regular, 18, QFont::Bold);
	QFont regular = Styles::font(Styles::Regular, 14);

	d->lblSignerHeader->setFont(header);
	d->lblSignatureHeader->setFont(header);
	d->lblNotice->setFont(Styles::font(Styles::Regular, 15));

	d->title->setFont(regular);
	d->info->setFont(regular);
	d->signatureView->header()->setFont(regular);
	d->lblRole->setFont(regular);
	d->lblSigningCity->setFont(regular);
	d->lblSigningCountry->setFont(regular);
	d->lblSigningCounty->setFont(regular);
	d->lblSigningZipCode->setFont(regular);
	d->signerCity->setFont(regular);
	d->signerState->setFont(regular);
	d->signerZip->setFont(regular);
	d->signerCountry->setFont(regular);
	d->signerRoles->setFont(regular);

	const QStringList l = s.locations();
	d->signerCity->setText( l.value( 0 ) );
	d->signerState->setText( l.value( 1 ) );
	d->signerZip->setText( l.value( 2 ) );
	d->signerCountry->setText( l.value( 3 ) );

	d->signerRoles->setText(s.roles().join(QStringLiteral(", ")));

	// Certificate info
	QTreeWidget *t = d->signatureView;
	t->header()->setSectionResizeMode(0, QHeaderView::ResizeToContents);

	QStringList horzHeaders { tr("Attribute"), tr("Value") };
	t->setHeaderLabels(horzHeaders);

	addItem( t, tr("Signer's Certificate issuer"), CertificateDetails::decodeCN(c.issuerInfo(QSslCertificate::CommonName)));
	addItem( t, tr("Signer's Certificate"), c );
	addItem( t, tr("Signature method"), QUrl( s.signatureMethod() ) );
	addItem( t, tr("Container format"), s.parent()->mediaType() );
	if( !s.profile().isEmpty() )
		addItem( t, tr("Signature format"), s.profile() );
	if( !s.policy().isEmpty() )
	{
		#define toVer(X) ((X)->toUInt() - 1)
		QStringList ver = s.policy().split('.');
		if( ver.size() >= 3 )
			addItem(t, tr("Signature policy"), QStringLiteral("%1.%2.%3").arg(toVer(ver.end()-3)).arg(toVer(ver.end()-2)).arg(toVer(ver.end()-1)));
		else
			addItem( t, tr("Signature policy"), s.policy() );
	}
	addItem( t, tr("Signed file count"), QString::number( s.parent()->documentModel()->rowCount() ) );
	if( !s.spuri().isEmpty() )
		addItem(t, QStringLiteral("SPUri"), QUrl(s.spuri()));

	if(!s.tsaTime().isNull())
	{
		SslCertificate tsa = s.tsaCert();
		addItem(t, tr("Archive Timestamp"), s.tsaTime().toLocalTime());
		addItem(t, tr("Archive Timestamp") + " (UTC)", s.tsaTime());
		addItem( t, tr("Archive TS Certificate issuer"), tsa.issuerInfo(QSslCertificate::CommonName) );
		addItem( t, tr("Archive TS Certificate"), tsa );
	}
	if(!s.tsTime().isNull())
	{
		SslCertificate ts = s.tsCert();
		addItem(t, tr("Signature Timestamp"), s.tsTime().toLocalTime());
		addItem(t, tr("Signature Timestamp") + " (UTC)", s.tsTime());
		addItem(t, tr("Hash value of signature"), SslCertificate::toHex(s.messageImprint()));
		addItem( t, tr("TS Certificate issuer"), ts.issuerInfo(QSslCertificate::CommonName) );
		addItem( t, tr("TS Certificate"), ts );
	}
	if(!s.ocspTime().isNull())
	{
		SslCertificate ocsp = s.ocspCert();
		addItem( t, tr("OCSP Certificate issuer"), ocsp.issuerInfo(QSslCertificate::CommonName) );
		addItem( t, tr("OCSP Certificate"), ocsp );
		if(s.tsTime().isNull())
			addItem(t, tr("Hash value of signature"), SslCertificate::toHex(s.messageImprint()));
		addItem(t, tr("OCSP time"), s.ocspTime().toLocalTime());
		addItem(t, tr("OCSP time") + " (UTC)", s.ocspTime());
	}
	addItem(t, tr("Signer's computer time (UTC)"), s.signTime());

#ifdef Q_OS_MAC
	t->setFont(Styles::font(Styles::Regular, 13));
#else
	t->setFont(regular);
#endif
}

SignatureDialog::~SignatureDialog() { delete d; }

void SignatureDialog::addItem(QTreeWidget *view, const QString &variable, QWidget *value)
{
	QTreeWidgetItem *i = new QTreeWidgetItem(view);
	QLabel *header = itemLabel(variable, view);
	setTabOrder(header, value);
	view->setItemWidget(i, 0, header);
	view->setItemWidget(i, 1, value);
	view->addTopLevelItem(i);
}

void SignatureDialog::addItem(QTreeWidget *view, const QString &variable, const QString &value)
{
	addItem(view, variable, itemLabel(value, view));
}

void SignatureDialog::addItem(QTreeWidget *view, const QString &variable, const QDateTime &value)
{
	addItem(view, variable, DateTime(value).toStringZ(QStringLiteral("dd.MM.yyyy hh:mm:ss")));
}

void SignatureDialog::addItem(QTreeWidget *view, const QString &variable, const QSslCertificate &value)
{
	SslCertificate c(value);
	QPushButton *button = itemButton(
		CertificateDetails::decodeCN(c.subjectInfo(QSslCertificate::CommonName)), view);
	connect(button, &QPushButton::clicked, this, [=]{ CertificateDetails::showCertificate(c, this); });
	addItem(view, variable, button);
}

void SignatureDialog::addItem(QTreeWidget *view, const QString &variable, const QUrl &value)
{
	QPushButton *button = itemButton(value.toString(), view);
	connect(button, &QPushButton::clicked, this, [=]{ QDesktopServices::openUrl( value ); });
	addItem(view, variable, button);
}

void SignatureDialog::decorateNotice(const QString &color)
{
	d->lblNotice->setText(QStringLiteral("<font color=\"%1\">%2</font>").arg(color, d->lblNotice->text()));
}

QPushButton* SignatureDialog::itemButton(const QString &text, QTreeWidget *view)
{
	QPushButton *button = new QPushButton(text, view);
#ifdef Q_OS_MAC
	QFont font = Styles::font(Styles::Regular, 13);
#else
	QFont font = Styles::font(Styles::Regular, 14);
#endif
	font.setUnderline(true);
	button->setFont(font);
	button->setStyleSheet(QStringLiteral("margin-left: 1px; border: none; text-align: left; color: #509B00"));
	return button;
}

QLabel* SignatureDialog::itemLabel(const QString &text, QTreeWidget *view)
{
	QLabel *label = new QLabel(text, view);
#ifdef Q_OS_MAC
	label->setFont(Styles::font(Styles::Regular, 13));
#else
	label->setFont(Styles::font(Styles::Regular, 14));
#endif
	label->setFocusPolicy(Qt::TabFocus);
	label->setStyleSheet(QStringLiteral("border: none;"));
	return label;
}
