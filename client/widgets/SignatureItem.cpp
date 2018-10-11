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
#include "dialogs/SignatureDialog.h"
#include "dialogs/WarningDialog.h"

#include <common/DateTime.h>
#include <common/SslCertificate.h>

#include <QFontMetrics>
#include <QPainter>
#include <QResizeEvent>
#include <QSvgWidget>
#include <QtCore/QTextStream>

using namespace ria::qdigidoc4;

class SignatureItem::Private: public Ui::SignatureItem
{
public:
	Private(DigiDocSignature s): signature(std::move(s)) {}
	DigiDocSignature signature;

	bool invalid;
	ria::qdigidoc4::WarningType error;
	QString nameText;
	QString serial;
	QString statusHtml;
	QString roleElided;
	QString status;
};

SignatureItem::SignatureItem(const DigiDocSignature &s, ContainerState /*state*/, bool isSupported, QWidget *parent)
: Item(parent)
, ui(new Private(s))
{
	ui->setupUi(this);
	ui->name->setFont(Styles::font(Styles::Regular, 14, QFont::DemiBold));
	ui->idSignTime->setFont(Styles::font(Styles::Regular, 11));
	ui->role->setFont(Styles::font(Styles::Regular, 11));
	ui->role->installEventFilter(this);
	ui->remove->setIcons(QStringLiteral("/images/icon_remove.svg"), QStringLiteral("/images/icon_remove_hover.svg"),
		QStringLiteral("/images/icon_remove_pressed.svg"), 1, 1, 17, 17);
	ui->remove->init(LabelButton::White, QString(), 0);
	ui->remove->setVisible(isSupported);
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

	QString accessibility, signingInfo;
	ui->nameText.clear();
	ui->serial.clear();
	ui->statusHtml.clear();
	ui->status.clear();
	ui->error = ria::qdigidoc4::NoWarning;

	QTextStream sa(&accessibility);
	QTextStream sc(&ui->statusHtml);
	QTextStream si(&signingInfo);
	
	auto signatureValidity = ui->signature.validate();

	ui->invalid = signatureValidity >= DigiDocSignature::Invalid;
	if(!cert.isNull())
		ui->nameText = cert.toString(cert.showCN() ? QStringLiteral("CN") : QStringLiteral("GN SN")).toHtmlEscaped();
	else
		ui->nameText = ui->signature.signedBy().toHtmlEscaped();

	bool isSignature = true;
	QString label = tr("Signature");
	if(ui->signature.profile() == QStringLiteral("TimeStampToken"))
	{
		isSignature = false;
		label = tr("Timestamp");
		ui->icon->setPixmap(QStringLiteral(":/images/icon_ajatempel.svg"));
	}
	else if(cert.type() & SslCertificate::TempelType)
		ui->icon->setPixmap(QStringLiteral(":/images/icon_digitempel.svg"));
	else
		ui->icon->setPixmap(QStringLiteral(":/images/icon_Allkiri_small.svg"));
	sa << label << " ";
	sc << "<span style=\"font-weight:normal;\">";
	switch( signatureValidity )
	{
	case DigiDocSignature::Valid:
		sa << tr("is valid", isSignature ? "Signature" : "Timestamp");
		sc << "<font color=\"green\">" << label << " " << tr("is valid", isSignature ? "Signature" : "Timestamp") << "</font>";
		break;
	case DigiDocSignature::Warning:
		sa << tr("is valid", isSignature ? "Signature" : "Timestamp") << " (" << tr("Warnings") << ")";
		sc << "<font color=\"green\">" << label << " " << tr("is valid", isSignature ? "Signature" : "Timestamp") << "</font> <font color=\"gold\">(" << tr("Warnings") << ")";
		break;
	case DigiDocSignature::NonQSCD:
		sa << tr("is valid", isSignature ? "Signature" : "Timestamp") << " (" << tr("Restrictions") << ")";
		sc << "<font color=\"green\">" << label << " " << tr("is valid", isSignature ? "Signature" : "Timestamp") << "</font> <font color=\"gold\">(" << tr("Restrictions") << ")";
		break;
	case DigiDocSignature::Test:
		sa << tr("is valid", isSignature ? "Signature" : "Timestamp") << " (" << tr("Test signature") << ")";
		sc << "<font color=\"green\">" << label << " " << tr("is valid", isSignature ? "Signature" : "Timestamp") << "</font> <font>(" << tr("Test signature") << ")";
		break;
	case DigiDocSignature::Invalid:
		if(isSignature)
			ui->error = ria::qdigidoc4::InvalidSignatureWarning;
		else
			ui->error = ria::qdigidoc4::InvalidTimestampWarning;
		sa << tr("is not valid", isSignature ? "Signature" : "Timestamp");
		sc << "<font color=\"red\">" << label << " " << tr("is not valid", isSignature ? "Signature" : "Timestamp");
		break;
	case DigiDocSignature::Unknown:
		if(isSignature)
			ui->error = ria::qdigidoc4::UnknownSignatureWarning;
		else
			ui->error = ria::qdigidoc4::UnknownTimestampWarning;
		sa << tr("is unknown", isSignature ? "Signature" : "Timestamp");
		sc << "<font color=\"red\">" << label << " " << tr("is unknown", isSignature ? "Signature" : "Timestamp");
		break;
	}
	sc << "</span>";
	updateNameField();
	ui->status = accessibility;

	if(!cert.isNull())
	{
		ui->serial = cert.toString(QStringLiteral("serialNumber")).toHtmlEscaped();
		sa << " " <<  ui->serial << " - ";
		si << ui->serial << " - ";
	}
	DateTime date(ui->signature.dateTime().toLocalTime());
	if( !date.isNull() )
	{
		sa << " " << tr("Signed on") << " "
			<< date.formatDate(QStringLiteral("dd. MMMM yyyy")) << " "
			<< tr("time") << " "
			<< date.toString(QStringLiteral("hh:mm"));
		si << tr("Signed on") << " "
			<< date.formatDate(QStringLiteral("dd. MMMM yyyy")) << " "
			<< tr("time") << " "
			<< date.toString(QStringLiteral("hh:mm"));
	}
	ui->idSignTime->setText(signingInfo);
	const QString role = ui->signature.role();
	ui->role->setHidden(role.isEmpty());
	ui->role->setText(role.toHtmlEscaped());
	ui->roleElided = ui->role->fontMetrics().elidedText(ui->role->text(), Qt::ElideRight, ui->role->geometry().width(), Qt::TextShowMnemonic);

	setAccessibleName(label + " " + cert.toString(cert.showCN() ? QStringLiteral("CN") : QStringLiteral("GN SN")));
	setAccessibleDescription( accessibility );
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
	return QWidget::event(event);
}

void SignatureItem::details()
{
	(new SignatureDialog(ui->signature, this))->open();
}

bool SignatureItem::eventFilter(QObject *o, QEvent *e)
{
	switch(e->type())
	{
	case QEvent::Paint:
		if(o == ui->role)
		{
			QPainter(qobject_cast<QLabel*>(o)).drawText(0, 0,
				ui->role->geometry().width(),
				ui->role->geometry().height(),
				ui->role->alignment(),
				ui->roleElided
			);
			return true;
		}
		break;
	case QEvent::Resize:
		if(QResizeEvent *r = static_cast<QResizeEvent*>(e))
		{
			if(o == ui->role)
				ui->roleElided = ui->role->fontMetrics().elidedText(ui->role->text().replace('\n', ' '), Qt::ElideRight, r->size().width(), Qt::TextShowMnemonic);
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

bool SignatureItem::isInvalid() const
{
	return ui->invalid;
}

bool SignatureItem::isSelfSigned(const QString& cardCode, const QString& mobileCode) const
{
	return !ui->serial.isEmpty() && (ui->serial == cardCode || ui->serial == mobileCode);
}

void SignatureItem::mouseReleaseEvent(QMouseEvent * /*event*/)
{
	details();
}

QString SignatureItem::red(const QString &text)
{
	return "<font color=\"red\">" + text + "</font>";
}

void SignatureItem::removeSignature()
{
	const SslCertificate c = ui->signature.cert();
	QString msg = tr("Remove signature %1")
		.arg(c.toString(c.showCN() ? QStringLiteral("CN serialNumber") : QStringLiteral("GN SN serialNumber")));

	WarningDialog dlg(msg, qApp->activeWindow());
	dlg.setCancelText(tr("CANCEL"));
	dlg.addButton(tr("OK"), SignatureRemove);
	if(dlg.exec() == SignatureRemove)
		emit remove(this);
}

void SignatureItem::updateNameField()
{
	if(ui->name->fontMetrics().width(ui->nameText  + " - " + ui->status) < ui->name->width())
		ui->name->setText((ui->invalid ? red(ui->nameText + " - ") : ui->nameText + " - ") + ui->statusHtml);
	else
		ui->name->setText((ui->invalid ? red(ui->nameText) : ui->nameText) + "<br/>" + ui->statusHtml);
}
