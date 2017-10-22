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
	setWindowFlags( Qt::Dialog | Qt::FramelessWindowHint );

	connect( ui->cancel, &QPushButton::clicked, this, &WarningDialog::reject );
	connect( this, &WarningDialog::finished, this, &WarningDialog::close );

	QFont regular(Styles::font(Styles::Regular, 14));
	ui->cancel->setFont(Styles::font(Styles::Condensed, 14));
	ui->text->setFont(regular);
	ui->text->setText(text);
	ui->text->setTextInteractionFlags(Qt::TextSelectableByMouse);
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
