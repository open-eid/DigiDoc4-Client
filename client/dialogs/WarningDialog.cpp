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

#include "WarningDialog.h"
#include "ui_WarningDialog.h"

#include "Application.h"

#include <QPushButton>
#include <QStyle>

WarningDialog::WarningDialog(const QString &text, const QString &details, QWidget *parent)
	: QDialog(parent)
	, ui(new Ui::WarningDialog)
{
	ui->setupUi(this);
	setWindowFlags(Qt::Dialog|Qt::CustomizeWindowHint);
	setAttribute(Qt::WA_DeleteOnClose);
#ifdef Q_OS_DARWIN
	setParent(parent, Qt::Sheet);
#endif

	ui->buttonBox->layout()->setSpacing(40);
	ui->text->setText(text);
	ui->details->setText(details);
	ui->details->setHidden(details.isEmpty());
	ui->showDetails->setHidden(details.isEmpty());
	cancel = ui->buttonBox->button(QDialogButtonBox::Close);
	cancel->setCursor(Qt::PointingHandCursor);
	connect(cancel, &QPushButton::clicked, this, &QDialog::reject);
	resetCancelStyle(true);

	if(!details.isEmpty())
	{
		ui->showDetails->init(false, tr("Details"), ui->details);
		connect(ui->showDetails, &AccordionTitle::closed, this, &WarningDialog::adjustSize);
		connect(ui->showDetails, &AccordionTitle::opened, this, &WarningDialog::adjustSize);
	}
	adjustSize();
}

WarningDialog::WarningDialog(const QString &text, QWidget *parent)
	: WarningDialog(text, {}, parent)
{}

WarningDialog::~WarningDialog()
{
	delete ui;
}

void WarningDialog::addButton(ButtonText label, int ret, bool red)
{
	addButton(buttonLabel(label), ret, red);
}

void WarningDialog::addButton(const QString& label, int ret, bool red)
{
	if(ui->buttonBox->buttons().size() > 3)
		ui->buttonBox->layout()->setSpacing(5);

	QDialogButtonBox::ButtonRole role = red ? QDialogButtonBox::RejectRole : [ret] {
		switch (ret) {
		case QDialogButtonBox::Ok:
		case QDialogButtonBox::Save:
		case QDialogButtonBox::Open:
		case QDialogButtonBox::SaveAll:
		case QDialogButtonBox::Retry:
		case QDialogButtonBox::Ignore:
			return QDialogButtonBox::AcceptRole;
		case QDialogButtonBox::Cancel:
		case QDialogButtonBox::Close:
		case QDialogButtonBox::Abort:
			return QDialogButtonBox::RejectRole;
		case QDialogButtonBox::Discard:
			return QDialogButtonBox::DestructiveRole;
		case QDialogButtonBox::Help:
			return QDialogButtonBox::HelpRole;
		case QDialogButtonBox::Apply:
			return QDialogButtonBox::ApplyRole;
		case QDialogButtonBox::Yes:
		case QDialogButtonBox::YesToAll:
			return QDialogButtonBox::YesRole;
		case QDialogButtonBox::No:
		case QDialogButtonBox::NoToAll:
			return QDialogButtonBox::NoRole;
		case QDialogButtonBox::RestoreDefaults:
		case QDialogButtonBox::Reset:
			return QDialogButtonBox::ResetRole;
		default:
			return QDialogButtonBox::ActionRole;
		}
	}();

	QPushButton *button = ui->buttonBox->addButton(label, role);
	button->setCursor(Qt::PointingHandCursor);
	switch(role) {
	case QDialogButtonBox::NoRole:
	case QDialogButtonBox::RejectRole:
	case QDialogButtonBox::ResetRole:
	case QDialogButtonBox::DestructiveRole:
		style()->unpolish(button);
		button->setProperty("warning", true);
		style()->polish(button);
		break;
	default:
		break;
	}
	connect(button, &QPushButton::clicked, this, [this, ret] { done(ret); });
}

QString WarningDialog::buttonLabel(ButtonText label)
{
	switch (label) {
	case NO: return tr("No");
	case OK: return QStringLiteral("OK");
	case Cancel: return tr("Cancel");
	case YES: return tr("Yes");
	case Remove: return tr("Remove");
	default: return {};
	}
}

void WarningDialog::resetCancelStyle(bool warning)
{
	style()->unpolish(cancel);
	cancel->setProperty("warning", warning);
	style()->polish(cancel);
}

void WarningDialog::setCancelText(ButtonText label)
{
	setCancelText(buttonLabel(label));
}

void WarningDialog::setCancelText(const QString &label)
{
	cancel->setText(label);
}

WarningDialog* WarningDialog::show(const QString &text, const QString &details)
{
	return show(Application::mainWindow(), text, details);
}

WarningDialog* WarningDialog::show(QWidget *parent, const QString &text, const QString &details)
{
	auto *dlg = new WarningDialog(text, details, parent);
	dlg->open();
	return dlg;
}
