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
#include "Styles.h"

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

	connect( ui->cancel, &QPushButton::clicked, this, &WarningDialog::reject );
	connect( this, &WarningDialog::finished, this, &WarningDialog::close );

	ui->cancel->setFont(Styles::font(Styles::Condensed, 14));
	ui->text->setFont(Styles::font(Styles::Regular, 14));
	ui->text->setText(text);
	ui->details->setFont(ui->text->font());
	ui->details->setText(details);
	ui->details->setHidden(details.isEmpty());
	ui->showDetails->setHidden(details.isEmpty());

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
	auto *button = new QPushButton(label, this);
	button->setAccessibleName(label.toLower());
	button->setCursor(ui->cancel->cursor());
	button->setFont(ui->cancel->font());
	button->setMinimumSize(
		std::max<int>(ui->cancel->minimumWidth(), ui->cancel->fontMetrics().boundingRect(label).width() + 16),
		ui->cancel->minimumHeight());

	if(red) {
		button->setProperty("warning", true);
#ifdef Q_OS_WIN // For Windows this button should be on the left side of the dialog window
		setLayoutDirection(Qt::RightToLeft);
#endif
	} else {
#ifdef Q_OS_UNIX // For macOS, Linux and BSD all positive buttons should be on the right side of the dialog window
		setLayoutDirection(Qt::RightToLeft);
#endif
	}

	connect(button, &QPushButton::clicked, this, [this, ret] { done(ret); });
	ui->buttonBarLayout->insertWidget(ui->buttonBarLayout->findChildren<QPushButton>().size() + 1, button);
}

QString WarningDialog::buttonLabel(ButtonText label)
{
	switch (label) {
	case NO: return tr("NO");
	case OK: return QStringLiteral("OK");
	case Cancel: return tr("CANCEL");
	case YES: return tr("YES");
	}
}

void WarningDialog::resetCancelStyle()
{
	style()->unpolish(ui->cancel);
	ui->cancel->setProperty("warning", false);
	style()->polish(ui->cancel);
}

void WarningDialog::setButtonSize(int width, int margin)
{
	ui->buttonBarLayout->setSpacing(margin);
	ui->cancel->setMinimumSize(width, ui->cancel->minimumHeight());
	ui->cancel->setMaximumSize(width, ui->cancel->minimumHeight());
}

void WarningDialog::setCancelText(ButtonText label)
{
	setCancelText(buttonLabel(label));
}

void WarningDialog::setCancelText(const QString& label)
{
	ui->cancel->setText(label);
	ui->cancel->setAccessibleName(label.toLower());
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
