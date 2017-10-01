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
#include <QLineEdit>
#include "Styles.h"


FirstRun::FirstRun(QWidget *parent) :
	QDialog(parent),
	ui(new Ui::FirstRun),
	page(None)
{
	ui->setupUi(this);
	setWindowFlags( Qt::Dialog | Qt::FramelessWindowHint );
	setWindowModality( Qt::ApplicationModal );

	ui->lang->addItem("Eesti keel");
	ui->lang->addItem("English");
	ui->lang->addItem("Русский язык");

	ui->lang->setFont(Styles::font(Styles::Regular, 18));
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

	ui->gotoSigning->hide();
	ui->gotoEncryption->hide();
	ui->gotoEid->hide();

	connect(ui->continue_2, &QPushButton::clicked, this,
			[this]()
			{
				setStyleSheet("image: url(:/images/FirstRun2.png);");

				ui->lang->hide();
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
				page = Signing;
				hideViewButtons();
			}
	);

	connect(ui->viewEncrypttion, &QPushButton::clicked, this,
			[this]()
			{
				page = Encryption;
				hideViewButtons();
			}
		);

	connect(ui->viewEid, &QPushButton::clicked, this,
			[this]()
			{
				page = MyEid;
				hideViewButtons();
			}
	);


	connect(ui->next, &QPushButton::clicked, this,
			[this]()
			{
				switch(page)
				{
				case Signing:
					page = Encryption;
					hideViewButtons();
					break;
				case Encryption:
					page = MyEid;
					hideViewButtons();
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

	connect(ui->gotoSigning, &QPushButton::clicked, this,
			[this]()
			{
				page = Signing;
				hideViewButtons();
			}
		);
	connect(ui->gotoEncryption, &QPushButton::clicked, this,
			[this]()
			{
				page = Encryption;
				hideViewButtons();
			}
		);
	connect(ui->gotoEid, &QPushButton::clicked, this,
			[this]()
			{
				page = MyEid;
				hideViewButtons();
			}
		);

}

void FirstRun::hideViewButtons()
{
	ui->viewSigning->hide();
	ui->viewEncrypttion->hide();
	ui->viewEid->hide();
	ui->next->show();

	ui->gotoSigning->show();
	ui->gotoEncryption->show();
	ui->gotoEid->show();

	QString normal = "border: none; image: url(:/images/icon_dot.png);";
	QString active = "border: none; image: url(:/images/icon_dot_active.png);";

	ui->gotoSigning->setStyleSheet(page == Signing ? active : normal);
	ui->gotoEncryption->setStyleSheet(page == Encryption ? active : normal);
	ui->gotoEid->setStyleSheet(page == MyEid ? active : normal);


	switch (page) {
	case Signing:
		setStyleSheet("image: url(:/images/FirstRunSigning.png);");
		ui->next->setText("VAATA JÄRGMIST TUTVUSTUST");
		ui->skip->show();
		break;
	case Encryption:
		setStyleSheet("image: url(:/images/FirstRunEnctypt.png);");
		ui->next->setText("VAATA JÄRGMIST TUTVUSTUST");
		ui->skip->show();
		break;
	default:
		setStyleSheet("image: url(:/images/FirstRunMyEID.png);");
		ui->next->setText("SISENE RAKENDUSSE");
		ui->skip->hide();
		break;
	}
}

FirstRun::~FirstRun()
{
	delete ui;
}
