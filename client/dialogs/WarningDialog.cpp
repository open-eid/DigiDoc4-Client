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
{
	ui->setupUi(this);
	setWindowFlags( Qt::Dialog | Qt::CustomizeWindowHint );

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

void WarningDialog::resetCancelStyle()
{
	ui->cancel->setStyleSheet(QString());
}

void WarningDialog::addButton(const QString& label, int ret, bool red)
{
	QPushButton *button = new QPushButton(label, this);
	button->setAccessibleName(label.toLower());
	button->setCursor(ui->cancel->cursor());
	button->setFont(ui->cancel->font());
	button->setMinimumSize(std::max<int>(ui->cancel->minimumWidth(), ui->cancel->fontMetrics().width(label) + 16), 34);

	if(red) {
		button->setStyleSheet(QStringLiteral(
			"QPushButton { border-radius: 2px; border: none; color: #ffffff; background-color: #981E32;}\n"
			"QPushButton:pressed { background-color: #F24A66; }\n"
			"QPushButton:hover:!pressed { background-color: #CD2541; }\n"
			"QPushButton:disabled {background-color: #BEDBED;}"));
	}

	connect(button, &QPushButton::clicked, [this, ret] {done(ret);});
	ui->buttonBarLayout->insertWidget(ui->buttonBarLayout->findChildren<QPushButton>().size() + 1, button);
}

void WarningDialog::setButtonSize(int width, int margin)
{
	ui->buttonBarLayout->setSpacing(margin);
	ui->cancel->setMinimumSize(width, 34);
	ui->cancel->setMaximumSize(width, 34);
}

void WarningDialog::setText(const QString& text)
{
	ui->text->setText(text);
	adjustSize();
}

void WarningDialog::warning(QWidget *parent, const QString& text)
{
	WarningDialog(text, QString(), parent).exec();
}
