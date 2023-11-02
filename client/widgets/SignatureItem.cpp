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
#include "DateTime.h"
#include "Styles.h"
#include "SslCertificate.h"
#include "dialogs/SignatureDialog.h"
#include "dialogs/WarningDialog.h"

#include <QtGui/QFontMetrics>
#include <QtGui/QKeyEvent>
#include <QtGui/QTextDocument>
#include <QSvgWidget>

using namespace ria::qdigidoc4;

class SignatureItem::Private: public Ui::SignatureItem
{
public:
	explicit Private(DigiDocSignature s): signature(std::move(s)) {}
	DigiDocSignature signature;

	ria::qdigidoc4::WarningType error = ria::qdigidoc4::NoWarning;
	QString nameText;
	QString serial;
	QString status;
	QString roleText;
	QString dateTime;
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
	ui->remove->setVisible(ui->signature.container()->isSupported());
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

	ui->serial.clear();
	ui->status.clear();
	ui->error = ria::qdigidoc4::NoWarning;
	if(!cert.isNull())
		ui->nameText = cert.toString(cert.showCN() ? QStringLiteral("CN") : QStringLiteral("GN SN")).toHtmlEscaped();
	else
		ui->nameText = ui->signature.signedBy().toHtmlEscaped();

	bool isSignature = true;
	if(ui->signature.profile() == QStringLiteral("TimeStampToken"))
	{
		isSignature = false;
		ui->icon->load(QStringLiteral(":/images/icon_ajatempel.svg"));
	}
	else if(cert.type() & SslCertificate::TempelType)
		ui->icon->load(QStringLiteral(":/images/icon_digitempel.svg"));
	else
		ui->icon->load(QStringLiteral(":/images/icon_Allkiri_small.svg"));
	QString isValid = isSignature ? tr("Signature is valid") : tr("Timestamp is valid");
	auto color = [](QLatin1String color, const QString &text) {
		return QStringLiteral("<font color=\"%1\">%2</font>").arg(color, text);
	};
	switch(ui->signature.status())
	{
	case DigiDocSignature::Valid:
		ui->status = color(QLatin1String("green"), isValid);
		break;
	case DigiDocSignature::Warning:
		ui->status = color(QLatin1String("green"), isValid) + ' ' + color(QLatin1String("#996C0B"), QStringLiteral("(%1)").arg(tr("Warnings")));
		break;
	case DigiDocSignature::NonQSCD:
		ui->status = color(QLatin1String("green"), isValid) + ' ' + color(QLatin1String("#996C0B"), QStringLiteral("(%1)").arg(tr("Restrictions")));
		break;
	case DigiDocSignature::Invalid:
		ui->error = isSignature ? ria::qdigidoc4::InvalidSignatureWarning : ria::qdigidoc4::InvalidTimestampWarning;
		ui->status = color(QLatin1String("red"), isSignature ? tr("Signature is not valid") : tr("Timestamp is not valid"));
		break;
	case DigiDocSignature::Unknown:
		ui->error = isSignature ? ria::qdigidoc4::UnknownSignatureWarning : ria::qdigidoc4::UnknownTimestampWarning;
		ui->status = color(QLatin1String("red"), isSignature ? tr("Signature is unknown") : tr("Timestamp is unknown"));
		break;
	}
	ui->status = QStringLiteral("<span style=\"font-weight:normal;\">%1</span>").arg(ui->status);

	if(!cert.isNull())
		ui->serial = cert.toString(QStringLiteral("serialNumber")).toHtmlEscaped();
	DateTime date(ui->signature.trustedTime().toLocalTime());
	if(!date.isNull())
	{
		ui->dateTime = QStringLiteral("%1 %2 %3 %4").arg(
			tr("Signed on"),
			date.formatDate(QStringLiteral("dd. MMMM yyyy")),
			tr("time"),
			date.toString(QStringLiteral("hh:mm")));
	}
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

bool SignatureItem::eventFilter(QObject *o, QEvent *e)
{
	auto details = [this]{ (new SignatureDialog(ui->signature, this))->open(); };
	switch(e->type())
	{
	case QEvent::MouseButtonRelease:
		details();
		return true;
	case QEvent::KeyRelease:
		if(auto *ke = static_cast<QKeyEvent*>(e))
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

void SignatureItem::initTabOrder(QWidget *item)
{
	setTabOrder(item, ui->name);
	setTabOrder(ui->name, lastTabWidget());
}

bool SignatureItem::isSelfSigned(const QString& cardCode, const QString& mobileCode) const
{
	return !ui->serial.isEmpty() && (ui->serial == cardCode || ui->serial == mobileCode);
}

QWidget* SignatureItem::lastTabWidget()
{
	return ui->remove;
}

void SignatureItem::removeSignature()
{
	const SslCertificate c = ui->signature.cert();
	QString msg = tr("Remove signature %1?")
		.arg(c.toString(c.showCN() ? QStringLiteral("CN serialNumber") : QStringLiteral("GN SN serialNumber")));

	auto *dlg = WarningDialog::show(this, msg);
	dlg->setCancelText(WarningDialog::Cancel);
	dlg->resetCancelStyle();
	dlg->addButton(WarningDialog::OK, SignatureRemove, true);
	connect(dlg, &WarningDialog::finished, this, [this](int result) {
		if(result == SignatureRemove)
			emit remove(this);
	});
}

void SignatureItem::updateNameField()
{
	QTextDocument doc;
	doc.setHtml(ui->status);
	QString plain = doc.toPlainText();
	auto red = [this](const QString &text) {
		return ui->signature.isInvalid() ? QStringLiteral("<font color=\"red\">%1</font>").arg(text) : text;
	};
	if(ui->name->fontMetrics().boundingRect(ui->nameText + " - " + plain).width() < ui->name->width())
		ui->name->setText(red(ui->nameText + " - ") + ui->status);
	else
		ui->name->setText(QStringLiteral("%1<br />%2").arg(red(ui->nameText), ui->status));
	ui->name->setAccessibleName(QStringLiteral("%1. %2. %3 %4")
		.arg(ui->nameText.toLower(), plain, ui->role->text(), ui->idSignTime->text()));
	ui->role->setText(ui->role->fontMetrics().elidedText(
		ui->roleText, Qt::ElideRight, ui->role->width() - 10, Qt::TextShowMnemonic));

	QStringList signingInfo{ui->serial, ui->dateTime};
	signingInfo.removeAll({});
	QString signingInfoSep = signingInfo.join(QStringLiteral(" - "));
	if(!signingInfoSep.isEmpty() && ui->idSignTime->fontMetrics().boundingRect(signingInfoSep).width() < ui->idSignTime->width())
		ui->idSignTime->setText(signingInfoSep);
	else
		ui->idSignTime->setText(signingInfo.join(QStringLiteral("<br />")));
}
