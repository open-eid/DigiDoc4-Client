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

#include "DateTime.h"
#include "SslCertificate.h"
#include "dialogs/CertificateDetails.h"
#include "effects/Overlay.h"

#include <QDesktopServices>

SignatureDialog::SignatureDialog(const DigiDocSignature &signature, QWidget *parent)
:	QDialog( parent )
,	s( signature )
,	d( new Ui::SignatureDialog )
{
	d->setupUi( this );
	setAttribute(Qt::WA_DeleteOnClose);
	setWindowFlags(windowFlags()|Qt::FramelessWindowHint);
	d->showErrors->hide();
	d->error->hide();
	connect(d->showErrors, &AccordionTitle::toggled, d->showRole, [this](bool open) {
		d->showRole->setChecked(!open);
		d->error->setVisible(open && !d->showErrors->isHidden());
	});
	connect(d->showRole, &AccordionTitle::toggled, d->showErrors, [this](bool open) {
		d->showErrors->setChecked(!open);
		d->role->setVisible(open);
	});

	new Overlay(this);

	bool isTS = signature.profile() == QLatin1String("TimeStampToken");
	d->warning->hide();
	switch(s.status())
	{
	case DigiDocSignature::Valid:
		d->status->setLabel(QStringLiteral("good"));
		d->status->setText(QCoreApplication::translate("SignatureItem", isTS ? "Timestamp is valid" : "Signature is valid"));
		break;
	case DigiDocSignature::Warning:
		d->status->setLabel(QStringLiteral("good"));
		d->status->setText(QCoreApplication::translate("SignatureItem", isTS ? "Timestamp is valid" : "Signature is valid"));
		d->warning->show();
		d->warning->setText(QCoreApplication::translate("SignatureItem", "Warnings"));
		d->lblNotice->setLabel(QStringLiteral("warning"));
		if(!s.lastError().isEmpty())
			d->error->setPlainText( s.lastError() );
		if(s.warning() & DigiDocSignature::DigestWeak )
			d->info->setText(tr(
				"The signature is technically correct, but it is based on the currently weak hash algorithm SHA-1, "
				"therefore it is not protected against forgery or alteration."));
		else
			d->info->setText(tr(
				"The signature is valid, but the container has a specific feature. Usually, this feature has arisen accidentally "
				"when containers were created. However, as it is not possible to edit a container without invalidating the signature, "
				"<a href='https://www.id.ee/en/article/digital-signing-and-electronic-signatures/'>a warning</a> is displayed."));
		break;
	case DigiDocSignature::NonQSCD:
		d->status->setLabel(QStringLiteral("good"));
		d->status->setText(QCoreApplication::translate("SignatureItem", isTS ? "Timestamp is valid" : "Signature is valid"));
		d->warning->show();
		d->warning->setText(QCoreApplication::translate("SignatureItem", "Restrictions"));
		d->lblNotice->setLabel(QStringLiteral("warning"));
		d->info->setText( tr(
			"This e-Signature is not equivalent with handwritten signature and therefore "
			"can be used only in transactions where Qualified e-Signature is not required.") );
		break;
	case DigiDocSignature::Invalid:
		d->status->setLabel(QStringLiteral("error"));
		d->status->setText(QCoreApplication::translate("SignatureItem", isTS ? "Timestamp is not valid" : "Signature is not valid"));
		d->error->setPlainText( s.lastError().isEmpty() ? tr("Unknown error") : s.lastError() );
		d->lblNotice->setLabel(QStringLiteral("error"));
		d->info->setText( tr(
			"This is an invalid signature or malformed digitally signed file. The signature is not valid.") );
		break;
	case DigiDocSignature::Unknown:
		d->status->setLabel(QStringLiteral("error"));
		d->status->setText(QCoreApplication::translate("SignatureItem", isTS ? "Timestamp is unknown" : "Signature is unknown"));
		d->error->setPlainText( s.lastError().isEmpty() ? tr("Unknown error") : s.lastError() );
		d->lblNotice->setLabel(QStringLiteral("error"));
		d->info->setText( tr(
			"Signature status is displayed \"unknown\" if you don't have all validity confirmation "
			"service certificates and/or certificate authority certificates installed into your computer "
			"(<a href='https://www.id.ee/en/article/digidoc4-client-error-ocsp-responder-is-not-trusted-no-certificate-for-this-responder-in-local-certstore/'>additional information</a>).") );
		break;
	}
	bool noError = d->error->toPlainText().isEmpty();
	if(noError && d->info->text().isEmpty())
	{
		d->lblNotice->hide();
		d->info->hide();
	}
	else if(!noError)
		d->showErrors->show();

	SslCertificate c = signature.cert();
	d->title->setText(!c.isNull() ? c.toString(c.showCN() ? QStringLiteral("CN serialNumber") : QStringLiteral("GN SN serialNumber")) : s.signedBy());
	connect(d->close, &QPushButton::clicked, this, &SignatureDialog::accept);

	const QStringList l = s.locations();
	d->signerCity->setText( l.value( 0 ) );
	d->signerState->setText( l.value( 1 ) );
	d->signerZip->setText( l.value( 2 ) );
	d->signerCountry->setText( l.value( 3 ) );

	d->signerRoles->setText(s.roles().join(QStringLiteral(", ")));
	auto setFocus = [](QLabel *lbl, QLabel *cnt) {
		lbl->setFocusPolicy(cnt->text().isEmpty() ? Qt::NoFocus : Qt::TabFocus);
		cnt->setFocusPolicy(cnt->text().isEmpty() ? Qt::NoFocus : Qt::TabFocus);
	};
	setFocus(d->labelCity, d->signerCity);
	setFocus(d->labelState, d->signerState);
	setFocus(d->labelCountry, d->signerCountry);
	setFocus(d->labelZip, d->signerZip);
	setFocus(d->labelRoles, d->signerRoles);

	// Certificate info
	QTreeWidget *t = d->signatureView;
	t->header()->setSectionResizeMode(0, QHeaderView::Fixed);
	t->header()->resizeSection(0, 244);

	auto addCert = [this](QTreeWidget *t, const QString &title, const QString &title2, const QSslCertificate &cert) {
		if(cert.isNull())
			return;
		addItem(t, title, cert);
		addItem(t, title2, cert.issuerInfo(QSslCertificate::CommonName).join(' '));
	};
	auto addTime = [this](QTreeWidget *t, const QString &title, const QDateTime &time) {
		if(time.isNull())
			return;
		addItem(t, title, time.toLocalTime());
		addItem(t, title + QStringLiteral(" (UTC)"), time);
	};

	addCert(t, tr("Signer's Certificate"), tr("Signer's Certificate issuer"), c);
	addItem(t, tr("Signature method"), QUrl(s.signatureMethod()));
	addItem(t, tr("Container format"), s.container()->mediaType());
	addItem(t, tr("Signature format"), s.profile());
	if( !s.policy().isEmpty() )
	{
		auto toVer = [](auto elem) { return elem->toUInt() - 1; };
		if(QStringList ver = s.policy().split('.'); ver.size() >= 3)
			addItem(t, tr("Signature policy"), QStringLiteral("%1.%2.%3").arg(toVer(ver.end()-3)).arg(toVer(ver.end()-2)).arg(toVer(ver.end()-1)));
		else
			addItem( t, tr("Signature policy"), s.policy() );
	}
	addItem(t, tr("Signed file count"), QString::number(s.container()->documentModel()->rowCount()));
	addItem(t, QStringLiteral("SPUri"), QUrl(s.spuri()));
	addTime(t, tr("Archive Timestamp"), s.tsaTime());
	addCert(t, tr("Archive TS Certificate"), tr("Archive TS Certificate issuer"), s.tsaCert());
	addTime(t, tr("Signature Timestamp"), s.tsTime());
	addCert(t, tr("TS Certificate"), tr("TS Certificate issuer"), s.tsCert());
	addItem(t, tr("Hash value of signature"), s.messageImprint().toHex(' ').toUpper());
	addCert(t, tr("OCSP Certificate"), tr("OCSP Certificate issuer"), s.ocspCert());
	addTime(t, tr("OCSP time"), s.ocspTime());
	addItem(t, tr("Signing time (UTC)"), s.trustedTime());
	addItem(t, tr("Claimed signing time (UTC)"), s.claimedTime());
}

SignatureDialog::~SignatureDialog()
{
	delete d;
}

void SignatureDialog::addItem(QTreeWidget *view, const QString &variable, QWidget *value)
{
	auto *i = new QTreeWidgetItem(view);
	QLabel *header = itemLabel(variable, view);
	view->setItemWidget(i, 0, header);
	view->setItemWidget(i, 1, value);
	view->addTopLevelItem(i);
	QWidget *prev = d->lblSignatureHeader;
	if(view->model()->rowCount() > 0)
		prev = view->itemWidget(view->itemAbove(i), 1);
	setTabOrder(prev, header);
	setTabOrder(header, value);
	setTabOrder(value, d->close);
}

void SignatureDialog::addItem(QTreeWidget *view, const QString &variable, const QString &value)
{
	if(!value.isEmpty())
		addItem(view, variable, itemLabel(value, view));
}

void SignatureDialog::addItem(QTreeWidget *view, const QString &variable, const QDateTime &value)
{
	addItem(view, variable, DateTime(value).toStringZ(QStringLiteral("dd.MM.yyyy hh:mm:ss")));
}

void SignatureDialog::addItem(QTreeWidget *view, const QString &variable, SslCertificate value)
{
	QPushButton *button = itemButton(
		value.toString(value.showCN() ? QStringLiteral("CN") : QStringLiteral("GN,SN,serialNumber")), view);
	connect(button, &QPushButton::clicked, this, [this, c = std::move(value)]{ CertificateDetails::showCertificate(c, this); });
	addItem(view, variable, button);
}

void SignatureDialog::addItem(QTreeWidget *view, const QString &variable, const QUrl &value)
{
	if(value.isEmpty())
		return;
	QPushButton *button = itemButton(value.toString(), view);
	connect(button, &QPushButton::clicked, this, [=]{ QDesktopServices::openUrl( value ); });
	addItem(view, variable, button);
}

QPushButton* SignatureDialog::itemButton(const QString &text, QTreeWidget *view)
{
	auto *button = new QPushButton(text, view);
	button->setCursor(QCursor(Qt::PointingHandCursor));
	return button;
}

QLabel* SignatureDialog::itemLabel(const QString &text, QTreeWidget *view)
{
	auto *label = new QLabel(text, view);
	label->setToolTip(text);
	label->setFocusPolicy(Qt::TabFocus);
	label->setTextInteractionFlags(Qt::TextBrowserInteraction);
	return label;
}
