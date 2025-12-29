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

#include "DateTime.h"
#include "SslCertificate.h"
#include "dialogs/SignatureDialog.h"
#include "dialogs/WarningDialog.h"

#include <QtGui/QKeyEvent>
#include <QtWidgets/QMessageBox>

using namespace ria::qdigidoc4;

struct SignatureItem::Private: public Ui::SignatureItem
{
	DigiDocSignature signature;

	ria::qdigidoc4::WarningType error = ria::qdigidoc4::NoWarning;
	QString serial;
	QString roleText;
};

SignatureItem::SignatureItem(DigiDocSignature s, QWidget *parent)
	: Item(parent)
	, ui(new Private{.signature = std::move(s)})
{
	ui->setupUi(this);
	ui->icon->installEventFilter(this);
	ui->name->installEventFilter(this);
	ui->status->installEventFilter(this);
	ui->warning->installEventFilter(this);
	ui->idSignTime->installEventFilter(this);
	ui->role->installEventFilter(this);
	ui->remove->setVisible(ui->signature.container()->isSupported());
	connect(ui->remove, &QToolButton::clicked, this, [this]{
		const SslCertificate c = ui->signature.cert();
		auto *dlg = new WarningDialog(tr("Remove signature %1?")
			.arg(c.toString(c.showCN() ? QStringLiteral("CN serialNumber") : QStringLiteral("GN SN serialNumber"))), this);
		dlg->setCancelText(WarningDialog::Cancel);
		dlg->resetCancelStyle(false);
		dlg->addButton(WarningDialog::Remove, QMessageBox::Ok, true);
		connect(dlg, &WarningDialog::finished, this, [this](int result) {
			if(result == QMessageBox::Ok)
				emit remove(this);
		});
		dlg->open();
	});
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
	ui->error = ria::qdigidoc4::NoWarning;
	QString nameText;
	if(!cert.isNull())
	{
		nameText = cert.toString(cert.showCN() ? QStringLiteral("CN") : QStringLiteral("GN SN")).toHtmlEscaped();
		ui->serial = cert.toString(QStringLiteral("serialNumber")).toHtmlEscaped();
	}
	else
		nameText = ui->signature.signedBy().toHtmlEscaped();

	bool isSignature = true;
	if(ui->signature.profile() == QLatin1String("TimeStampToken"))
	{
		isSignature = false;
		ui->icon->load(QStringLiteral(":/images/icon_ajatempel.svg"));
	}
	else if(cert.type() & SslCertificate::TempelType)
		ui->icon->load(QStringLiteral(":/images/icon_digitempel.svg"));
	else
		ui->icon->load(QStringLiteral(":/images/icon_Allkiri_small.svg"));
	ui->warning->hide();
	switch(ui->signature.status())
	{
	case DigiDocSignature::Valid:
		ui->status->setLabel(QStringLiteral("good"));
		ui->status->setText(isSignature ? tr("Signature is valid") : tr("Timestamp is valid"));
		break;
	case DigiDocSignature::Warning:
		ui->status->setLabel(QStringLiteral("good"));
		ui->status->setText(isSignature ? tr("Signature is valid") : tr("Timestamp is valid"));
		ui->warning->show();
		ui->warning->setText(tr("Warnings"));
		break;
	case DigiDocSignature::NonQSCD:
		ui->status->setLabel(QStringLiteral("good"));
		ui->status->setText(isSignature ? tr("Signature is valid") : tr("Timestamp is valid"));
		ui->warning->show();
		ui->warning->setText(tr("Restrictions"));
		break;
	case DigiDocSignature::Invalid:
		ui->status->setLabel(QStringLiteral("error"));
		ui->status->setText(isSignature ? tr("Signature is not valid") : tr("Timestamp is not valid"));
		ui->error = isSignature ? ria::qdigidoc4::InvalidSignatureError : ria::qdigidoc4::InvalidTimestampError;
		break;
	case DigiDocSignature::Unknown:
		ui->status->setLabel(QStringLiteral("error"));
		ui->status->setText(isSignature ? tr("Signature is unknown") : tr("Timestamp is unknown"));
		ui->error = isSignature ? ria::qdigidoc4::UnknownSignatureWarning : ria::qdigidoc4::UnknownTimestampWarning;
		break;
	}

	if(DateTime date(ui->signature.trustedTime().toLocalTime()); !date.isNull())
	{
		ui->idSignTime->setText(QStringLiteral("%1 %2 %3 %4").arg(
			tr("Signed on"),
			date.formatDate(QStringLiteral("dd. MMMM yyyy")),
			tr("time"),
			date.toString(QStringLiteral("hh:mm"))));
	}
	else
		ui->idSignTime->hide();
	ui->roleText = ui->signature.role().replace('\n', ' ');
	ui->role->setHidden(ui->roleText.isEmpty());
	ui->name->setText(QStringList{nameText, ui->serial}.join(QStringLiteral(", ")));
	ui->name->setAccessibleName(QStringLiteral("%1. %2. %3 %4")
		.arg(ui->name->text().toLower(), ui->status->text(), ui->role->text(), ui->idSignTime->text()));
	elideRole();
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
		elideRole();
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

bool SignatureItem::isSelfSigned(const QString& cardCode) const
{
	return !ui->serial.isEmpty() && ui->serial == cardCode;
}

QWidget* SignatureItem::lastTabWidget()
{
	return ui->remove;
}

void SignatureItem::elideRole()
{
	ui->role->setText(ui->role->fontMetrics().elidedText(
		ui->roleText, Qt::ElideRight, ui->role->width() - 10, Qt::TextShowMnemonic));
}
