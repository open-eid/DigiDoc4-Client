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

#include "SignatureItem.h"
#include "ui_SignatureItem.h"

#include "Application.h"
#include "Styles.h"
#include "SslCertificate.h"
#include "dialogs/SignatureDialog.h"
#include "dialogs/WarningDialog.h"

#include <common/DateTime.h>

#include <QtCore/QTextStream>
#include <QtGui/QFontMetrics>
#include <QtGui/QPainter>
#include <QtGui/QResizeEvent>
#include <QtGui/QTextDocument>
#include <QtSvg/QSvgWidget>

using namespace ria::qdigidoc4;

class SignatureItem::Private: public Ui::SignatureItem
{
public:
	explicit Private(DigiDocSignature s): signature(std::move(s)) {}
	DigiDocSignature signature;

	bool invalid = true;
	ria::qdigidoc4::WarningType error = ria::qdigidoc4::NoWarning;
	QString nameText;
	QString serial;
	QString status;
	QString roleText;
};

SignatureItem::SignatureItem(DigiDocSignature s, ContainerState /*state*/, QWidget *parent)
: Item(parent)
, ui(new Private(std::move(s)))
{
	ui->setupUi(this);
	ui->icon->installEventFilter(this);
	ui->name->setFont(Styles::font(Styles::Regular, 14, QFont::DemiBold));
	ui->name->installEventFilter(this);
	ui->idSignTime->setFont(Styles::font(Styles::Regular, 11));
	ui->idSignTime->installEventFilter(this);
	ui->role->setFont(Styles::font(Styles::Regular, 11));
	ui->role->installEventFilter(this);
	ui->remove->setIcons(QStringLiteral("/images/icon_remove.svg"), QStringLiteral("/images/icon_remove_hover.svg"),
		QStringLiteral("/images/icon_remove_pressed.svg"), 17, 17);
	ui->remove->init(LabelButton::White, {}, 0);
	ui->remove->setVisible(s.parent()->isSupported());
	connect(ui->remove, &LabelButton::clicked, this, &SignatureItem::removeSignature);
	init();
}

SignatureItem::~SignatureItem()
{
	delete ui;
}

void SignatureItem::init()
{
	const SslCertificate cert = ui->signature.cert();
	DigiDocSignature::SignatureStatus signatureValidity = ui->signature.validate();

	ui->serial.clear();
	ui->status.clear();
	ui->error = ria::qdigidoc4::NoWarning;
	ui->invalid = signatureValidity >= DigiDocSignature::Invalid;
	if(!cert.isNull())
		ui->nameText = cert.toString(cert.showCN() ? QStringLiteral("CN") : QStringLiteral("GN SN")).toHtmlEscaped();
	else
		ui->nameText = ui->signature.signedBy().toHtmlEscaped();

	QTextStream s(&ui->status);
	bool isSignature = true;
	QString label = tr("Signature");
	if(ui->signature.profile() == QStringLiteral("TimeStampToken"))
	{
		isSignature = false;
		label = tr("Timestamp");
		ui->icon->load(QStringLiteral(":/images/icon_ajatempel.svg"));
	}
	else if(cert.type() & SslCertificate::TempelType)
		ui->icon->load(QStringLiteral(":/images/icon_digitempel.svg"));
	else
		ui->icon->load(QStringLiteral(":/images/icon_Allkiri_small.svg"));
	s << "<span style=\"font-weight:normal;\">";
	auto isValid = [&isSignature] {
		return isSignature ? tr("is valid", "Signature") : tr("is valid", "Timestamp");
	};
	auto isNotValid = [&isSignature] {
		return isSignature ? tr("is not valid", "Signature") : tr("is not valid", "Timestamp");
	};
	auto isUnknown = [&isSignature] {
		return isSignature ? tr("is unknown", "Signature") : tr("is unknown", "Timestamp");
	};
	switch( signatureValidity )
	{
	case DigiDocSignature::Valid:
		s << "<font color=\"green\">" << label << " " << isValid() << "</font>";
		break;
	case DigiDocSignature::Warning:
		s << "<font color=\"green\">" << label << " " << isValid() << "</font> <font color=\"996C0B\">(" << tr("Warnings") << ")";
		break;
	case DigiDocSignature::NonQSCD:
		s << "<font color=\"green\">" << label << " " << isValid() << "</font> <font color=\"996C0B\">(" << tr("Restrictions") << ")";
		break;
	case DigiDocSignature::Test:
		s << "<font color=\"green\">" << label << " " << isValid() << "</font> <font>(" << tr("Test signature") << ")";
		break;
	case DigiDocSignature::Invalid:
		ui->error = isSignature ? ria::qdigidoc4::InvalidSignatureWarning : ria::qdigidoc4::InvalidTimestampWarning;
		s << "<font color=\"red\">" << label << " " << isNotValid();
		break;
	case DigiDocSignature::Unknown:
		ui->error = isSignature ? ria::qdigidoc4::UnknownSignatureWarning : ria::qdigidoc4::UnknownTimestampWarning;
		s << "<font color=\"red\">" << label << " " << isUnknown();
		break;
	}
	s << "</span>";

	QString signingInfo;
	QTextStream si(&signingInfo);
	if(!cert.isNull())
	{
		ui->serial = cert.toString(QStringLiteral("serialNumber")).toHtmlEscaped();
		si << ui->serial << " - ";
	}
	DateTime date(ui->signature.trustedTime().toLocalTime());
	if( !date.isNull() )
	{
		si << tr("Signed on") << " "
			<< date.formatDate(QStringLiteral("dd. MMMM yyyy")) << " "
			<< tr("time") << " "
			<< date.toString(QStringLiteral("hh:mm"));
	}
	ui->idSignTime->setText(signingInfo);
	ui->roleText = ui->signature.role().replace('\n', ' ');
	ui->role->setHidden(ui->roleText.isEmpty());
	updateNameField();
}

bool SignatureItem::event(QEvent *event)
{
	switch(event->type())
	{
	case QEvent::LanguageChange:
		ui->retranslateUi(this);
		init();
		break;
	case QEvent::Resize:
		updateNameField();
		break;
	default: break;
	}
	return Item::event(event);
}

void SignatureItem::details()
{
	(new SignatureDialog(ui->signature, this))->open();
}

bool SignatureItem::eventFilter(QObject *o, QEvent *e)
{
	switch(e->type())
	{
	case QEvent::MouseButtonRelease:
		details();
		return true;
	case QEvent::KeyRelease:
		if(QKeyEvent *ke = static_cast<QKeyEvent*>(e))
		{
			if(isEnabled() && (ke->key() == Qt::Key_Enter || ke->key() == Qt::Key_Space))
				details();
		}
		break;
	default: break;
	}
	return Item::eventFilter(o, e);
}

ria::qdigidoc4::WarningType SignatureItem::getError() const
{
	return ui->error;
}

QString SignatureItem::id() const
{
	return ui->signature.id();
}

QWidget* SignatureItem::initTabOrder(QWidget *item)
{
	setTabOrder(item, ui->name);
	setTabOrder(ui->name, ui->remove);
	return  ui->remove;
}

bool SignatureItem::isInvalid() const
{
	return ui->invalid;
}

bool SignatureItem::isSelfSigned(const QString& cardCode, const QString& mobileCode) const
{
	return !ui->serial.isEmpty() && (ui->serial == cardCode || ui->serial == mobileCode);
}

QString SignatureItem::red(const QString &text, bool paint) const
{
	return paint ? QStringLiteral("<font color=\"red\">%1</font>").arg(text) : text;
}

void SignatureItem::removeSignature()
{
	const SslCertificate c = ui->signature.cert();
	QString msg = tr("Remove signature %1")
		.arg(c.toString(c.showCN() ? QStringLiteral("CN serialNumber") : QStringLiteral("GN SN serialNumber")));

	WarningDialog *dlg = new WarningDialog(msg, this);
	dlg->setCancelText(tr("CANCEL"));
	dlg->resetCancelStyle();
	dlg->addButton(tr("OK"), SignatureRemove, true);
	if(dlg->exec() == SignatureRemove)
		emit remove(this);
}

void SignatureItem::updateNameField()
{
	QTextDocument doc;
	doc.setHtml(ui->status);
	QString plain = doc.toPlainText();
	if(ui->name->fontMetrics().boundingRect(ui->nameText  + " - " + plain).width() < ui->name->width())
		ui->name->setText(red(ui->nameText + " - ", ui->invalid) + ui->status);
	else
		ui->name->setText(QStringLiteral("%1<br />%2").arg(red(ui->nameText, ui->invalid), ui->status));
	ui->name->setAccessibleName(QStringLiteral("%1. %2 %3").arg(plain, ui->role->text(), ui->idSignTime->text()));
	ui->role->setText(ui->role->fontMetrics().elidedText(
		ui->roleText, Qt::ElideRight, ui->role->width() - 10, Qt::TextShowMnemonic));
}
