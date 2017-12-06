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

#include <common/Settings.h>

#include <QKeyEvent>
#include <QPixmap>
#include <QSvgWidget>

FirstRun::FirstRun(QWidget *parent) :
	QDialog(parent),
	ui(new Ui::FirstRun)
{
	ui->setupUi(this);
	setWindowFlags( Qt::Dialog | Qt::FramelessWindowHint );
	setWindowModality( Qt::ApplicationModal );

	auto buttonFont = Styles::font(Styles::Condensed, 14);
	auto labelFont = Styles::font(Styles::Regular, 18);
	auto dmLabelFont = Styles::font(Styles::Regular, 18);
	auto regular12 = Styles::font(Styles::Regular, 12);
	auto regular14 = Styles::font(Styles::Regular, 14);
	auto titleFont = Styles::font(Styles::Regular, 20, QFont::DemiBold);

	//	Page 1: language
	ui->title->setFont(Styles::font(Styles::Regular, 20, QFont::Bold));
	ui->welcome->setFont(titleFont);
	ui->intro->setFont(regular14);
	ui->langLabel->setFont(regular12);
	ui->lang->setFont(labelFont);

	ui->lang->setFont(Styles::font(Styles::Regular, 18));
	ui->lang->addItem("Eesti keel");
	ui->lang->addItem("English");
	ui->lang->addItem("Русский язык");

	if(Settings::language() == "en")
		ui->lang->setCurrentIndex(1);
	else if(Settings::language() == "ru")
		ui->lang->setCurrentIndex(2);
	else
		ui->lang->setCurrentIndex(0);

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

			ui->retranslateUi(this);
		}
		);
	ui->continueBtn->setFont(buttonFont);
	connect(ui->continueBtn, &QPushButton::clicked, this, [this](){ui->stack->setCurrentIndex(Intro);});

	QSvgWidget* coatOfArs = new QSvgWidget(":/images/Logo_Suur.svg", ui->coatOfArms);
	coatOfArs->show();
	QSvgWidget* leaves = new QSvgWidget(":/images/vapilehed.svg", ui->leaves);
	leaves->show();
	QSvgWidget* structureFunds = new QSvgWidget(":/images/Struktuurifondid.svg", ui->structureFunds);
	structureFunds->show();

	// Page 2: intro
	ui->introTitle->setFont(titleFont);
	ui->labelSign->setFont(dmLabelFont);
	ui->labelCrypto->setFont(dmLabelFont);
	ui->labelEid->setFont(dmLabelFont);
	ui->signIntro->setFont(regular14);
	ui->cryptoIntro->setFont(regular14);
	ui->eidIntro->setFont(regular14);
	ui->skip->setFont(regular12);
	connect(ui->skip, &QPushButton::clicked, this, [this](){ emit close(); });

	ui->viewSigning->setFont(buttonFont);
	ui->viewEncryption->setFont(buttonFont);
	ui->viewEid->setFont(buttonFont);
	connect(ui->viewSigning, &QPushButton::clicked, this, [this](){ui->stack->setCurrentIndex(Signing);});
	connect(ui->viewEncryption, &QPushButton::clicked, this, [this](){ui->stack->setCurrentIndex(Encryption);});
	connect(ui->viewEid, &QPushButton::clicked, this, [this](){ui->stack->setCurrentIndex(MyEid);});

	QSvgWidget* signIcon = new QSvgWidget(":/images/icon_Allkiri_hover.svg", ui->signWidget);
	signIcon->resize(150, 110);
	signIcon->move(0, 0);
	signIcon->show();
	ui->signWidget->show();
	QSvgWidget* cryptoIcon = new QSvgWidget(":/images/icon_Krypto_hover.svg", ui->cryptoWidget);
	cryptoIcon->resize(106, 117);
	cryptoIcon->move(0, 0);
	cryptoIcon->show();
	QSvgWidget* eidIcon = new QSvgWidget(":/images/icon_Minu_eID_hover.svg", ui->eidWidget);
	eidIcon->resize(138, 97);
	eidIcon->move(0, 0);
	eidIcon->show();

	// Page 3: Signing
	ui->signTitle->setFont(titleFont);
	ui->labelSign1->setFont(dmLabelFont);
	ui->labelSign2->setFont(dmLabelFont);
	ui->labelSign3->setFont(dmLabelFont);
	ui->textSign1->setFont(regular14);
	ui->textSign2->setFont(regular14);
	ui->textSign3->setFont(regular14);
	ui->skip_2->setFont(regular12);
	connect(ui->skip_2, &QPushButton::clicked, this, [this](){ emit close(); });

	ui->next->setFont(buttonFont);
	connect(ui->next, &QPushButton::clicked, this, [this](){ui->stack->setCurrentIndex(Encryption);});

	QPixmap sign1 = QPixmap(":/images/intro_sign-select.png");
	ui->signImage1->setPixmap(sign1.scaled(ui->signImage1->size(), Qt::IgnoreAspectRatio, Qt::SmoothTransformation));
	QPixmap sign2 = QPixmap(":/images/intro_sign-sign.png");
	ui->signImage2->setPixmap(sign2.scaled(ui->signImage2->size(), Qt::IgnoreAspectRatio, Qt::SmoothTransformation));
	QPixmap sign3 = QPixmap(":/images/intro_sign-pin.png");
	ui->signImage3->setPixmap(sign3.scaled(ui->signImage3->size(), Qt::IgnoreAspectRatio, Qt::SmoothTransformation));
	QPixmap one = QPixmap(":/images/intro_one.svg");
	QPixmap two = QPixmap(":/images/intro_two.svg");
	QPixmap three = QPixmap(":/images/intro_three.svg");
	ui->signOne->setPixmap(one.scaled(32, 32, Qt::IgnoreAspectRatio, Qt::SmoothTransformation));
	ui->signTwo->setPixmap(two.scaled(32, 32, Qt::IgnoreAspectRatio, Qt::SmoothTransformation));
	ui->signThree->setPixmap(three.scaled(32, 32, Qt::IgnoreAspectRatio, Qt::SmoothTransformation));
	connect(ui->gotoEncryption, &QPushButton::clicked, this, [this](){ui->stack->setCurrentIndex(Encryption);});
	connect(ui->gotoEid, &QPushButton::clicked, this, [this](){ui->stack->setCurrentIndex(MyEid);});

	// Page 4: Crypto
	ui->cryptoTitle->setFont(titleFont);
	ui->labelCrypto1->setFont(dmLabelFont);
	ui->labelCrypto2->setFont(dmLabelFont);
	ui->labelCrypto3->setFont(dmLabelFont);
	ui->textCrypto1->setFont(regular14);
	ui->textCrypto2->setFont(regular14);
	ui->textCrypto3->setFont(regular14);
	ui->skip_3->setFont(regular12);
	connect(ui->skip_3, &QPushButton::clicked, this, [this](){ emit close(); });

	ui->next_2->setFont(buttonFont);
	connect(ui->next_2, &QPushButton::clicked, this, [this](){ui->stack->setCurrentIndex(MyEid);});

	QPixmap crypto1 = QPixmap(":/images/intro_crypto-select.png");
	ui->cryptoImage1->setPixmap(crypto1.scaled(ui->cryptoImage1->size(), Qt::IgnoreAspectRatio, Qt::SmoothTransformation));
	QPixmap crypto2 = QPixmap(":/images/intro_crypto-recipient.png");
	ui->cryptoImage2->setPixmap(crypto2.scaled(ui->cryptoImage2->size(), Qt::IgnoreAspectRatio, Qt::SmoothTransformation));
	QPixmap crypto3 = QPixmap(":/images/intro_crypto-encrypt.png");
	ui->cryptoImage3->setPixmap(crypto3.scaled(ui->cryptoImage3->size(), Qt::IgnoreAspectRatio, Qt::SmoothTransformation));
	ui->cryptoOne->setPixmap(one.scaled(32, 32, Qt::IgnoreAspectRatio, Qt::SmoothTransformation));
	ui->cryptoTwo->setPixmap(two.scaled(32, 32, Qt::IgnoreAspectRatio, Qt::SmoothTransformation));
	ui->cryptoThree->setPixmap(three.scaled(32, 32, Qt::IgnoreAspectRatio, Qt::SmoothTransformation));
	connect(ui->gotoSigning_2, &QPushButton::clicked, this, [this](){ui->stack->setCurrentIndex(Signing);});
	connect(ui->gotoEid_2, &QPushButton::clicked, this, [this](){ui->stack->setCurrentIndex(MyEid);});

	// Page 5: My eID
	ui->eidTitle->setFont(titleFont);
	ui->labelEid1->setFont(dmLabelFont);
	ui->labelEid2->setFont(dmLabelFont);
	ui->labelEid3->setFont(dmLabelFont);
	ui->textEid1->setFont(regular14);
	ui->textEid2->setFont(regular14);
	ui->textEid3->setFont(regular14);

	ui->enter->setFont(buttonFont);
	connect(ui->enter, &QPushButton::clicked, this, [this](){ emit close(); });

	QPixmap eid1 = QPixmap(":/images/intro_eid-manage.png");
	ui->eidImage1->setProperty("PICTURE", eid1);
	ui->eidImage1->setPixmap(eid1.scaled(298, 216, Qt::IgnoreAspectRatio, Qt::SmoothTransformation));
	QPixmap eid2 = QPixmap(":/images/intro_eid-other.png");
	ui->eidImage2->setProperty("PICTURE", eid2);
	ui->eidImage2->setPixmap(eid2.scaled(298, 216, Qt::IgnoreAspectRatio, Qt::SmoothTransformation));
	QPixmap eid3 = QPixmap(":/images/intro_eid-info.png");
	ui->eidImage3->setProperty("PICTURE", eid3);
	ui->eidImage3->setPixmap(eid3.scaled(298, 216, Qt::IgnoreAspectRatio, Qt::SmoothTransformation));
	connect(ui->gotoSigning_3, &QPushButton::clicked, this, [this](){ui->stack->setCurrentIndex(Signing);});
	connect(ui->gotoEncryption_3, &QPushButton::clicked, this, [this](){ui->stack->setCurrentIndex(Encryption);});
}

FirstRun::~FirstRun()
{
	delete ui;
}

void FirstRun::keyPressEvent(QKeyEvent *event)
{
	int next = ui->stack->currentIndex();
	switch(event->key())
	{
	case Qt::Key_Left:
		next = ui->stack->currentIndex() - 1;
		if(next < Language)
			next = Language;
		break;
	case Qt::Key_Right:
		next = ui->stack->currentIndex() + 1;
		if(next > MyEid)
			next = MyEid;
	default:
		QDialog::keyPressEvent(event);
		break;
	}

	if(next != ui->stack->currentIndex())
		ui->stack->setCurrentIndex(next);
}