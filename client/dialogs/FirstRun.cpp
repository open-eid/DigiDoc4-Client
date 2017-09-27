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


#include "FirstRun.h"
#include "ui_FirstRun.h"
#include "Styles.h"


FirstRun::FirstRun(QWidget *parent) :
	QDialog(parent),
	ui(new Ui::FirstRun),
	page(Signing)
{
	ui->setupUi(this);

	ui->continue_2->setFont(Styles::font(Styles::Condensed, 14));
	ui->viewSigning->setFont(Styles::font(Styles::Condensed, 14));
	ui->viewEncrypttion->setFont(Styles::font(Styles::Condensed, 14));
	ui->viewEid->setFont(Styles::font(Styles::Condensed, 14));
	ui->next->setFont(Styles::font(Styles::Condensed, 14));
	ui->skip->setFont(Styles::font(Styles::Condensed, 12));

	ui->viewSigning->hide();
	ui->viewEncrypttion->hide();
	ui->viewEid->hide();
	ui->next->hide();
	ui->skip->hide();

	connect(ui->continue_2, &QPushButton::clicked, this,
			[this]()
			{
				setStyleSheet("image: url(:/images/FirstRun2.png);");

				ui->continue_2->hide();

				ui->viewSigning->show();
				ui->viewEncrypttion->show();
				ui->viewEid->show();
				ui->skip->show();
			}
	);

	connect(ui->viewSigning, &QPushButton::clicked, this,
			[this]()
			{
				setStyleSheet("image: url(:/images/FirstRunSigning.png);");
				hideViewButtons();
				page = Signing;
			}
	);

	connect(ui->viewEncrypttion, &QPushButton::clicked, this,
			[this]()
			{
				setStyleSheet("image: url(:/images/FirstRunEnctypt.png);");
				hideViewButtons();
				page = Encryption;
			}
		);

	connect(ui->viewEid, &QPushButton::clicked, this,
			[this]()
			{
				setStyleSheet("image: url(:/images/FirstRunMyEID.png);");
				hideViewButtons(false);
				page = MyEid;
			}
	);


	connect(ui->next, &QPushButton::clicked, this,
			[this]()
			{
				switch(page)
				{
				case Signing:
					setStyleSheet("image: url(:/images/FirstRunEnctypt.png);");
					hideViewButtons();
					page = Encryption;
					break;
				case Encryption:
					setStyleSheet("image: url(:/images/FirstRunMyEID.png);");
					hideViewButtons(false);
					page = MyEid;
					break;
				default:
					emit close();
					break;
				}
			}
		);

	connect(ui->skip, &QPushButton::clicked, this,
			[this]()
			{
				emit close();
			}
		);
}

void FirstRun::hideViewButtons(bool showSkipe)
{
	if(!showSkipe)
		ui->next->setText("SISENE RAKENDUSSE");

	ui->continue_2->hide();
	ui->viewSigning->hide();
	ui->viewEncrypttion->hide();
	ui->viewEid->hide();

	ui->next->show();
	if(showSkipe)
		ui->skip->show();
	else
		ui->skip->hide();
}

FirstRun::~FirstRun()
{
	delete ui;
}
