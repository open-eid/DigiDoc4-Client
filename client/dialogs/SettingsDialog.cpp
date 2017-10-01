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


#include "SettingsDialog.h"
#include "ui_SettingsDialog.h"
#include "effects/Overlay.h"
#include "Styles.h"

SettingsDialog::SettingsDialog(QWidget *parent) :
	QDialog(parent),
	ui(new Ui::SettingsDialog)
{
	ui->setupUi(this);
	setWindowFlags( Qt::Dialog | Qt::FramelessWindowHint );
	setWindowModality( Qt::ApplicationModal );

	QFont headerFont = Styles::font( Styles::Regular, 18 );
	QFont regularFont = Styles::font( Styles::Regular, 14 );
	QFont condensed12 = Styles::font( Styles::Condensed, 12 );
	ui->label_2->setFont(headerFont);
	ui->pushButton->setFont(condensed12);
	ui->pushButton_2->setFont(condensed12);
	ui->pushButton_3->setFont(condensed12);
	ui->pushButton_4->setFont(condensed12);
	ui->pushButton_5->setFont(condensed12);
	ui->pushButton_6->setFont(condensed12);


	ui->label_3->setFont(headerFont);
	ui->label_4->setFont(headerFont);
	ui->label_5->setFont(headerFont);

	ui->estonian->setFont(regularFont);
	ui->russian->setFont(regularFont);
	ui->english->setFont(regularFont);

	ui->sameDirectory->setFont(regularFont);
	ui->specifyDirectory->setFont(regularFont);
	ui->choosDirectory->setFont(regularFont);
	ui->directory->setFont(regularFont);

	ui->checkUpdatePeriod->setFont(regularFont);
	ui->checkTslRefresh->setFont(regularFont);

	ui->version->setFont(Styles::font( Styles::Regular, 12 ));
	ui->fromFile->setFont(condensed12);
	ui->fromHistory->setFont(condensed12);
	ui->close->setFont(Styles::font( Styles::Condensed, 14 ));


	connect( ui->close, &QPushButton::clicked, this, &SettingsDialog::accept );
	connect( this, &SettingsDialog::finished, this, &SettingsDialog::close );

	connect( ui->pushButton,   &QPushButton::clicked, this, [this](){ changePage(ui->pushButton);    ui->stackedWidget->setCurrentIndex(0); } );
	connect( ui->pushButton_2, &QPushButton::clicked, this, [this](){ changePage(ui->pushButton_2);  ui->stackedWidget->setCurrentIndex(1); } );
	connect( ui->pushButton_3, &QPushButton::clicked, this, [this](){ changePage(ui->pushButton_3);  ui->stackedWidget->setCurrentIndex(1); } );
	connect( ui->pushButton_4, &QPushButton::clicked, this, [this](){ changePage(ui->pushButton_4);  ui->stackedWidget->setCurrentIndex(1); } );
	connect( ui->pushButton_5, &QPushButton::clicked, this, [this](){ changePage(ui->pushButton_5);  ui->stackedWidget->setCurrentIndex(1); } );
	connect( ui->pushButton_6, &QPushButton::clicked, this, [this](){ changePage(ui->pushButton_6);  ui->stackedWidget->setCurrentIndex(1); } );

	ui->checkUpdatePeriod->addItem("Kord päevas");
	ui->checkUpdatePeriod->addItem("Kord nädalas");
	ui->checkUpdatePeriod->addItem("Kord kuus");
}

SettingsDialog::~SettingsDialog()
{
	delete ui;
}


void SettingsDialog::changePage(QPushButton* button)
{
	if(button->isChecked())
	{
		ui->pushButton->setChecked(false);
		ui->pushButton_2->setChecked(false);
		ui->pushButton_3->setChecked(false);
		ui->pushButton_4->setChecked(false);
		ui->pushButton_5->setChecked(false);
		ui->pushButton_6->setChecked(false);
	}

	button->setChecked(true);
}

int SettingsDialog::exec()
{
	Overlay overlay( parentWidget() );
	overlay.show();
	auto rc = QDialog::exec();
	overlay.close();

	return rc;
}
