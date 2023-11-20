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
#include "Styles.h"
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
	d->showErrors->init(false, tr("TECHNICAL INFORMATION"), d->error);
	d->showErrors->hide();
	d->showRole->init(true, tr("ROLE AND ADDRESS"), d->role);
	d->error->hide();
	connect(d->showErrors, &AccordionTitle::opened, d->showRole, &AccordionTitle::openSection);
	connect(d->showErrors, &AccordionTitle::closed, this, [this] { d->showRole->setSectionOpen(); });
	connect(d->showRole, &AccordionTitle::opened, d->showErrors, &AccordionTitle::openSection);
	connect(d->showRole, &AccordionTitle::closed, this, [this] { d->showErrors->setSectionOpen(); });

	new Overlay(this);

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
	switch(s.status())
	{
	case DigiDocSignature::Valid:
		status += isValid(isTS);
		break;
	case DigiDocSignature::Warning:
		status += QStringLiteral("%1</font> <font color=\"#996C0B\">(%2)").arg(isValid(isTS), tr("Warnings"));
		decorateNotice(QStringLiteral("#996C0B"));
		if(!s.lastError().isEmpty())
			d->error->setPlainText( s.lastError() );
		if(s.warning() & DigiDocSignature::DigestWeak )
			d->info->setText( tr(
				"The signature is technically correct, but it is based on the currently weak hash algorithm SHA-1, therefore it is not protected against forgery or alteration.") );
		else
			d->info->setText(tr("SIGNATURE_WARNING"));
		break;
	case DigiDocSignature::NonQSCD:
		status += QStringLiteral("%1</font> <font color=\"#996C0B\">(%2)").arg(isValid(isTS), tr("Restrictions"));
		decorateNotice(QStringLiteral("#996C0B"));
		d->info->setText( tr(
			"This e-Signature is not equivalent with handwritten signature and therefore "
			"can be used only in transactions where Qualified e-Signature is not required.") );
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
	auto setFocus = [](QLabel *lbl, QLabel *cnt) {
		lbl->setFocusPolicy(cnt->text().isEmpty() ? Qt::NoFocus : Qt::TabFocus);
		cnt->setFocusPolicy(cnt->text().isEmpty() ? Qt::NoFocus : Qt::TabFocus);
	};
	setFocus(d->lblSigningCity, d->signerCity);
	setFocus(d->lblSigningCounty, d->signerState);
	setFocus(d->lblSigningCountry, d->signerCountry);
	setFocus(d->lblSigningZipCode, d->signerZip);
	setFocus(d->lblRole, d->signerRoles);

	// Certificate info
	QTreeWidget *t = d->signatureView;
	t->header()->setSectionResizeMode(0, QHeaderView::Fixed);
	t->header()->resizeSection(0, 244);
	t->setHeaderLabels({ tr("Attribute"), tr("Value") });

	auto addCert = [this](QTreeWidget *t, const QString &title, const QString &title2, const SslCertificate &cert) {
		if(cert.isNull())
			return;
		addItem(t, title, cert);
		addItem(t, title2, cert.issuerInfo(QSslCertificate::CommonName));
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
		#define toVer(X) ((X)->toUInt() - 1)
		QStringList ver = s.policy().split('.');
		if( ver.size() >= 3 )
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
	addItem(t, tr("Hash value of signature"), SslCertificate::toHex(s.messageImprint()));
	addCert(t, tr("OCSP Certificate"), tr("OCSP Certificate issuer"), s.ocspCert());
	addTime(t, tr("OCSP time"), s.ocspTime());
	addItem(t, tr("Signing time (UTC)"), s.trustedTime());
	addItem(t, tr("Claimed signing time (UTC)"), s.claimedTime());

#ifdef Q_OS_MAC
	t->setFont(Styles::font(Styles::Regular, 13));
#else
	t->setFont(regular);
#endif
}

SignatureDialog::~SignatureDialog()
{
	delete d;
}

void SignatureDialog::addItem(QTreeWidget *view, const QString &variable, QWidget *value)
{
	QTreeWidgetItem *i = new QTreeWidgetItem(view);
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

void SignatureDialog::addItem(QTreeWidget *view, const QString &variable, const QSslCertificate &value)
{
	SslCertificate c(value);
	QPushButton *button = itemButton(
		c.toString(c.showCN() ? QStringLiteral("CN") : QStringLiteral("GN,SN,serialNumber")), view);
	connect(button, &QPushButton::clicked, this, [=]{ CertificateDetails::showCertificate(c, this); });
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
	button->setCursor(QCursor(Qt::PointingHandCursor));
	button->setStyleSheet(QStringLiteral("margin-left: 1px; border: none; text-align: left; color: #509B00"));
	return button;
}

QLabel* SignatureDialog::itemLabel(const QString &text, QTreeWidget *view)
{
	QLabel *label = new QLabel(text, view);
	label->setToolTip(text);
#ifdef Q_OS_MAC
	label->setFont(Styles::font(Styles::Regular, 13));
#else
	label->setFont(Styles::font(Styles::Regular, 14));
#endif
	label->setFocusPolicy(Qt::TabFocus);
	label->setStyleSheet(QStringLiteral("border: none;"));
	label->setTextInteractionFlags(Qt::TextBrowserInteraction);
	return label;
}
