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

#include "Styles.h"

WarningDialog::WarningDialog(const QString &text, const QString &details, QWidget *parent)
: QDialog(parent)
, ui(new Ui::WarningDialog)
, buttonOffset(2)
{
	ui->setupUi(this);
	setWindowFlags( Qt::Dialog | Qt::FramelessWindowHint );

	connect( ui->cancel, &QPushButton::clicked, this, &WarningDialog::reject );
	connect( this, &WarningDialog::finished, this, &WarningDialog::close );

	QFont regular(Styles::font(Styles::Regular, 14));
	ui->cancel->setFont(Styles::font(Styles::Condensed, 14));
	ui->text->setFont(regular);
	ui->text->setText(text);
	ui->text->setTextInteractionFlags(ui->text->textInteractionFlags() | Qt::TextSelectableByMouse);

	if(details.isNull())
	{
		ui->details->hide();
		ui->showDetails->hide();
	}
	else
	{
		ui->details->setFont(regular);
		ui->details->setText(details);
		ui->details->setTextInteractionFlags(Qt::TextSelectableByMouse);
		ui->showDetails->borderless();
		ui->showDetails->show();
		ui->showDetails->init(false, "Details", ui->details);
		connect(ui->showDetails, &AccordionTitle::opened, this, &WarningDialog::adjustSize);
	}
	adjustSize();

#ifdef Q_OS_WIN32
	setStyleSheet("QWidget#WarningDialog{ border-radius: 2px; background-color: #FFFFFF; border: solid #D9D9D8; border-width: 1px 1px 1px 1px;}");
#endif
}

WarningDialog::WarningDialog(const QString &text, QWidget *parent)
: WarningDialog(text, QString(), parent)
{
}

WarningDialog::~WarningDialog()
{
	delete ui;
}

void WarningDialog::setCancelText(const QString& label)
{
	ui->cancel->setText(label);
}

void WarningDialog::addButton(const QString& label, int ret)
{
	auto layout = qobject_cast<QBoxLayout*>(ui->buttonBar->layout());
	layout->insertSpacing(buttonOffset++, 35);

	QPushButton *button = new QPushButton(label, this);
	button->setMinimumSize(120, 34);
	button->setStyleSheet("QPushButton {border-radius: 2px; border: none;color: #ffffff;background-color: #006EB5;}\nQPushButton:pressed {background-color: #41B6E6;} "
		"QPushButton:hover:!pressed {background-color: #008DCF;}\nQPushButton:disabled {background-color: #BEDBED;}");
	button->setCursor(Qt::PointingHandCursor);
	button->setFont(Styles::font(Styles::Condensed, 14));
	connect(button, &QPushButton::clicked, [this, ret]() {done(ret);});
	layout->insertWidget(buttonOffset++, button);
}
