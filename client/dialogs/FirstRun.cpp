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

#include <common/Common.h>

#include <QKeyEvent>
#include <QPixmap>
#include <QTextStream>


FirstRun::FirstRun(QWidget *parent) :
	QDialog(parent),
	ui(new Ui::FirstRun),
	dragged(false)
{
	ui->setupUi(this);
	setWindowFlags( Qt::Dialog | Qt::FramelessWindowHint );
	setWindowModality( Qt::ApplicationModal );
	setFixedSize( size() );
	setCursor(Qt::OpenHandCursor);

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

	ui->lang->setFont(Styles::font(Styles::Regular, 18));
	ui->lang->addItem(QStringLiteral("Eesti keel"));
	ui->lang->addItem(QStringLiteral("English"));
	ui->lang->addItem("Русский язык"); //QStringLiteral breaks windows text

	if(Common::language() == QStringLiteral("en"))
		ui->lang->setCurrentIndex(1);
	else if(Common::language() == QStringLiteral("ru"))
		ui->lang->setCurrentIndex(2);
	else
		ui->lang->setCurrentIndex(0);

	connect( ui->lang, static_cast<void(QComboBox::*)(int)>(&QComboBox::currentIndexChanged), this, [this](int index) {
		switch(index)
		{
		case 1:
			emit langChanged(QStringLiteral("en"));
			break;
		case 2:
			emit langChanged(QStringLiteral("ru"));
			break;
		default:
			emit langChanged(QStringLiteral("et"));
			break;
		}

		ui->retranslateUi(this);
		loadImages();
	});
	ui->continueBtn->setFont(buttonFont);
	connect(ui->continueBtn, &QPushButton::clicked, this, [this]{ui->stack->setCurrentIndex(Intro);});

	QSvgWidget* coatOfArs = new QSvgWidget(QStringLiteral(":/images/Logo_Suur.svg"), ui->coatOfArms);
	coatOfArs->show();
	ui->leaves->load(QStringLiteral(":/images/vapilehed.svg"));
	ui->structureFunds->load(QStringLiteral(":/images/Struktuurifondid.svg"));

	// Page 2: intro
	ui->introTitle->setFont(titleFont);
	ui->labelSign->setFont(dmLabelFont);
	ui->labelCrypto->setFont(dmLabelFont);
	ui->labelEid->setFont(dmLabelFont);
	ui->signIntro->setFont(regular14);
	ui->cryptoIntro->setFont(regular14);
	ui->eidIntro->setFont(regular14);
	ui->skip->setFont(regular12);
	connect(ui->skip, &QPushButton::clicked, this, [this]{ emit close(); });

	ui->viewSigning->setFont(buttonFont);
	ui->viewEncryption->setFont(buttonFont);
	ui->viewEid->setFont(buttonFont);
	connect(ui->viewSigning, &QPushButton::clicked, this, [this]{ui->stack->setCurrentIndex(Signing);});
	connect(ui->viewEncryption, &QPushButton::clicked, this, [this]{ui->stack->setCurrentIndex(Encryption);});
	connect(ui->viewEid, &QPushButton::clicked, this, [this]{ui->stack->setCurrentIndex(MyEid);});

	ui->introPageLayout->itemAtPosition(1, 0)->setAlignment(Qt::AlignCenter);
	ui->introPageLayout->itemAtPosition(1, 1)->setAlignment(Qt::AlignCenter);
	ui->introPageLayout->itemAtPosition(1, 2)->setAlignment(Qt::AlignCenter);
	ui->signWidget->load(QStringLiteral(":/images/icon_Allkiri_hover.svg"));
	ui->cryptoWidget->load(QStringLiteral(":/images/icon_Krypto_hover.svg"));
	ui->eidWidget->load(QStringLiteral(":/images/icon_Minu_eID_hover.svg"));

	// Page 3: Signing
	ui->signTitle->setFont(titleFont);
	ui->labelSign1->setFont(dmLabelFont);
	ui->labelSign2->setFont(dmLabelFont);
	ui->labelSign3->setFont(dmLabelFont);
	ui->textSign1->setFont(regular14);
	ui->textSign2->setFont(regular14);
	ui->textSign3->setFont(regular14);
	ui->skip_2->setFont(regular12);
	connect(ui->skip_2, &QPushButton::clicked, this, [this]{ emit close(); });

	ui->next->setFont(buttonFont);
	connect(ui->next, &QPushButton::clicked, this, [this]{ui->stack->setCurrentIndex(Encryption);});

	QPixmap one = QPixmap(QStringLiteral(":/images/intro_one.svg"));
	QPixmap two = QPixmap(QStringLiteral(":/images/intro_two.svg"));
	QPixmap three = QPixmap(QStringLiteral(":/images/intro_three.svg"));
	ui->signOne->setPixmap(one.scaled(32, 32, Qt::IgnoreAspectRatio, Qt::SmoothTransformation));
	ui->signTwo->setPixmap(two.scaled(32, 32, Qt::IgnoreAspectRatio, Qt::SmoothTransformation));
	ui->signThree->setPixmap(three.scaled(32, 32, Qt::IgnoreAspectRatio, Qt::SmoothTransformation));
	connect(ui->gotoEncryption, &QPushButton::clicked, this, [this]{ui->stack->setCurrentIndex(Encryption);});
	connect(ui->gotoEid, &QPushButton::clicked, this, [this]{ui->stack->setCurrentIndex(MyEid);});

	// Page 4: Crypto
	ui->cryptoTitle->setFont(titleFont);
	ui->labelCrypto1->setFont(dmLabelFont);
	ui->labelCrypto2->setFont(dmLabelFont);
	ui->labelCrypto3->setFont(dmLabelFont);
	ui->textCrypto1->setFont(regular14);
	ui->textCrypto2->setFont(regular14);
	ui->textCrypto3->setFont(regular14);
	ui->skip_3->setFont(regular12);
	connect(ui->skip_3, &QPushButton::clicked, this, [this]{ emit close(); });

	ui->next_2->setFont(buttonFont);
	connect(ui->next_2, &QPushButton::clicked, this, [this]{ui->stack->setCurrentIndex(MyEid);});

	ui->cryptoOne->setPixmap(one.scaled(32, 32, Qt::IgnoreAspectRatio, Qt::SmoothTransformation));
	ui->cryptoTwo->setPixmap(two.scaled(32, 32, Qt::IgnoreAspectRatio, Qt::SmoothTransformation));
	ui->cryptoThree->setPixmap(three.scaled(32, 32, Qt::IgnoreAspectRatio, Qt::SmoothTransformation));
	connect(ui->gotoSigning_2, &QPushButton::clicked, this, [this]{ui->stack->setCurrentIndex(Signing);});
	connect(ui->gotoEid_2, &QPushButton::clicked, this, [this]{ui->stack->setCurrentIndex(MyEid);});

	// Page 5: My eID
	ui->eidTitle->setFont(titleFont);
	ui->labelEid1->setFont(dmLabelFont);
	ui->labelEid3->setFont(dmLabelFont);
	ui->textEid1->setFont(regular14);
	ui->textEid3->setFont(regular14);

	ui->enter->setFont(buttonFont);
	connect(ui->enter, &QPushButton::clicked, this, [this]{ emit close(); });

	connect(ui->gotoSigning_3, &QPushButton::clicked, this, [this]{ui->stack->setCurrentIndex(Signing);});
	connect(ui->gotoEncryption_3, &QPushButton::clicked, this, [this]{ui->stack->setCurrentIndex(Encryption);});

	loadImages();
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
		break;
	default:
		QDialog::keyPressEvent(event);
		break;
	}

	if(next != ui->stack->currentIndex())
		ui->stack->setCurrentIndex(next);
}

void FirstRun::mouseMoveEvent(QMouseEvent *event)
{
	if (!dragged)
		return;

	const QPoint delta = event->globalPos() - lastPos;
	move(x()+delta.x(), y()+delta.y());
	lastPos = event->globalPos();
}

void FirstRun::mousePressEvent(QMouseEvent *event)
{
	setCursor(Qt::ClosedHandCursor);
	dragged = true;
	lastPos = event->globalPos();
}

void FirstRun::mouseReleaseEvent(QMouseEvent * /*event*/)
{
	setCursor(Qt::OpenHandCursor);
	dragged = false;
}

void FirstRun::loadPixmap(const QString &base, const QString &lang, QLabel *parent)
{
	QString resource;
 	QTextStream st(&resource);

	st << ":/images/" << base << '_' << lang << ".png";
	parent->setPixmap(QPixmap(resource).scaled(parent->size(), Qt::IgnoreAspectRatio, Qt::SmoothTransformation));
}

void FirstRun::loadImages()
{
	QString lang = Common::language();

	loadPixmap(QStringLiteral("intro_sign-select"), lang, ui->signImage1);
	loadPixmap(QStringLiteral("intro_sign-sign"), lang, ui->signImage2);
	loadPixmap(QStringLiteral("intro_sign-pin"), lang, ui->signImage3);
	loadPixmap(QStringLiteral("intro_crypto-select"), lang, ui->cryptoImage1);
	loadPixmap(QStringLiteral("intro_crypto-recipient"), lang, ui->cryptoImage2);
	loadPixmap(QStringLiteral("intro_crypto-encrypt"), lang, ui->cryptoImage3);
	loadPixmap(QStringLiteral("intro_eid-manage"), lang, ui->eidImage1);
	loadPixmap(QStringLiteral("intro_eid-info"), lang, ui->eidImage3);
}
