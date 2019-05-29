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
, buttonMargin(35)
, buttonOffset(2)
, buttonWidth(120)
{
	ui->setupUi(this);
	setWindowFlags( Qt::Dialog | Qt::CustomizeWindowHint );

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
		ui->showDetails->setClosable(true);
		ui->showDetails->show();
		ui->showDetails->init(false, tr("Details"), tr("Details"), ui->details);
		connect(ui->showDetails, &AccordionTitle::closed, this, &WarningDialog::adjustSize);
		connect(ui->showDetails, &AccordionTitle::opened, this, &WarningDialog::adjustSize);
	}
	adjustSize();
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
	ui->cancel->setAccessibleName(label.toLower());
}

void WarningDialog::addButton(const QString& label, int ret)
{
	auto layout = qobject_cast<QBoxLayout*>(ui->buttonBar->layout());
	layout->insertSpacing(buttonOffset++, buttonMargin);

	QFont font = Styles::font(Styles::Condensed, 14);
	QFontMetrics fm(font);

	QPushButton *button = new QPushButton(label, this);
	button->setAccessibleName(label.toLower());
	button->setCursor(Qt::PointingHandCursor);
	button->setFont(font);

	int width = buttonWidth;
	int textWidth = fm.width(label);
	if(textWidth > (buttonWidth - 5))
		width = textWidth + 16;
	button->setMinimumSize(width, 34);
	button->setStyleSheet(QStringLiteral(
		"QPushButton {border-radius: 2px; border: none;color: #ffffff;background-color: #006EB5;}\n"
		"QPushButton:pressed {background-color: #41B6E6;}\n"
		"QPushButton:hover:!pressed {background-color: #008DCF;}\n"
		"QPushButton:disabled {background-color: #BEDBED;}"));

	connect(button, &QPushButton::clicked, [this, ret]() {done(ret);});
	layout->insertWidget(buttonOffset++, button);
}

void WarningDialog::setButtonSize(int width, int margin)
{
	ui->cancel->setMinimumSize(width, 34);
	ui->cancel->setMaximumSize(width, 34);
	buttonWidth = width;
	buttonMargin = margin;
}

void WarningDialog::setText(const QString& text)
{
	ui->text->setText(text);
	adjustSize();
}

void WarningDialog::warning(QWidget *parent, const QString& text)
{
	WarningDialog dlg(text, QString(), parent);
	dlg.exec();
}
