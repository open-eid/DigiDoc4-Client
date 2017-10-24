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
#include "common/Settings.h"

#include <QDebug>
#include <QKeyEvent>
#include <QLineEdit>


bool ArrowKeyFilter::eventFilter(QObject* obj, QEvent* event)
{
	if (event->type()==QEvent::KeyPress)
	{
		QKeyEvent* key = static_cast<QKeyEvent*>(event);
		if ( key->key()==Qt::Key_Left || key->key() == Qt::Key_Right )
		{
			FirstRun *dlg = qobject_cast<FirstRun*>( obj );
			if( dlg )
			{
				dlg->navigate( key->key()==Qt::Key_Right );
				return true;
			}
		}
	}

	return QObject::eventFilter(obj, event);
}



FirstRun::FirstRun(QWidget *parent) :
	QDialog(parent),
	ui(new Ui::FirstRun),
	page(Language)
{
	ui->setupUi(this);
	setWindowFlags( Qt::Dialog | Qt::FramelessWindowHint );
	setWindowModality( Qt::ApplicationModal );

	installEventFilter( new ArrowKeyFilter );

	ui->lang->addItem("Eesti keel");
	ui->lang->addItem("English");
	ui->lang->addItem("Русский язык");

	if(Settings::language() == "en")
	{
		ui->lang->setCurrentIndex(1);
	}
	else if(Settings::language() == "ru")
	{
		ui->lang->setCurrentIndex(2);
	}
	else
	{
		ui->lang->setCurrentIndex(0);
	}

	connect( ui->lang, static_cast<void(QComboBox::*)(int)>(&QComboBox::currentIndexChanged), this,
		[this](int index)
		{
			switch(index)
			{
			case 1:
				emit langChanged("en");
				break;
			case 2:
				emit langChanged("ru");
				break;
			default:
				emit langChanged("et");
				break;
			}
		}
			);


	ui->lang->setFont(Styles::font(Styles::Regular, 18));
	ui->continue_2->setFont(Styles::font(Styles::Condensed, 14));
	ui->viewSigning->setFont(Styles::font(Styles::Condensed, 14));
	ui->viewEncryption->setFont(Styles::font(Styles::Condensed, 14));
	ui->viewEid->setFont(Styles::font(Styles::Condensed, 14));
	ui->next->setFont(Styles::font(Styles::Condensed, 14));
	ui->skip->setFont(Styles::font(Styles::Condensed, 12));

	ui->viewSigning->hide();
	ui->viewEncryption->hide();
	ui->viewEid->hide();

	ui->next->hide();
	ui->skip->hide();

	ui->gotoSigning->hide();
	ui->gotoEncryption->hide();
	ui->gotoEid->hide();

	connect(ui->continue_2, &QPushButton::clicked, this,
			[this]()
			{
				toPage( Intro );
			}
	);

	connect(ui->viewSigning, &QPushButton::clicked, this,
			[this]()
			{
				toPage( Signing );
			}
	);

	connect(ui->viewEncryption, &QPushButton::clicked, this,
			[this]()
			{
				toPage( Encryption );
			}
		);

	connect(ui->viewEid, &QPushButton::clicked, this,
			[this]()
			{
				toPage( MyEid );
			}
	);


	connect(ui->next, &QPushButton::clicked, this,
			[this]()
			{
				navigate( true );
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
				toPage( Signing );
			}
		);
	connect(ui->gotoEncryption, &QPushButton::clicked, this,
			[this]()
			{
				toPage( Encryption );
			}
		);
	connect(ui->gotoEid, &QPushButton::clicked, this,
			[this]()
			{
				toPage( MyEid );
			}
		);
}

void FirstRun::toPage( View toPage )
{
	page = toPage;
	if( toPage == Language )
	{
		ui->lang->show();
		ui->continue_2->show();

		ui->viewSigning->hide();
		ui->viewEncryption->hide();
		ui->viewEid->hide();
		ui->next->hide();
		ui->skip->hide();
		ui->gotoSigning->hide();
		ui->gotoEncryption->hide();
		ui->gotoEid->hide();
	
		setStyleSheet("image: url(:/images/FirstRun1.png);");
	}
	else
	{
		ui->lang->hide();
		ui->continue_2->hide();

		if( toPage == Intro)
		{
			ui->viewSigning->show();
			ui->viewEncryption->show();
			ui->viewEid->show();
			ui->skip->show();

			ui->next->hide();
			ui->gotoSigning->hide();
			ui->gotoEncryption->hide();
			ui->gotoEid->hide();
			setStyleSheet("image: url(:/images/FirstRun2.png);");
		}
		else
		{
			ui->viewSigning->hide();
			ui->viewEncryption->hide();
			ui->viewEid->hide();
			ui->next->show();
		
			ui->gotoSigning->show();
			ui->gotoEncryption->show();
			ui->gotoEid->show();
			showDetails();
		}
	}
}

void FirstRun::showDetails()
{
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
		setStyleSheet("image: url(:/images/FirstRunEncrypt.png);");
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

void FirstRun::navigate( bool forward )
{
	if( forward )
	{
		if( page == MyEid )
		{
			emit close();
			return;
		}
		toPage( static_cast<View>(page + 1) );
	}
	else if( page != Language )
	{
		toPage( static_cast<View>(page - 1) );
	}
}
