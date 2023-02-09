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

#include "Settings.h"
#include "Styles.h"

#include <QKeyEvent>
#include <QPixmap>

#include <algorithm>

FirstRun::FirstRun(QWidget *parent)
	: QDialog(parent)
	, ui(new Ui::FirstRun)
{
	ui->setupUi(this);
	setWindowFlag(Qt::FramelessWindowHint);
	setAttribute(Qt::WA_DeleteOnClose);
	setFixedSize( size() );
	setCursor(Qt::OpenHandCursor);
	if(parent)
		move(parent->geometry().center() - geometry().center());

	auto buttonFont = Styles::font(Styles::Condensed, 14);
	auto dmLabelFont = Styles::font(Styles::Regular, 18);
	auto regular12 = Styles::font(Styles::Regular, 12);
	auto regular14 = Styles::font(Styles::Regular, 14);
	auto titleFont = Styles::font(Styles::Regular, 20, QFont::DemiBold);

	//	Page 1: language
	ui->title->setFont(Styles::font(Styles::Regular, 20, QFont::Bold));
	ui->welcome->setFont(titleFont);
	ui->intro->setFont(regular14);
	ui->langLabel->setFont(regular12);

	ui->lang->setFont(regular14);
	ui->lang->addItem(QStringLiteral("Eesti keel"));
	ui->lang->addItem(QStringLiteral("English"));
	ui->lang->addItem(QString::fromUtf8("Русский язык")); //QStringLiteral breaks windows text

	if(Settings::LANGUAGE == QLatin1String("en"))
		ui->lang->setCurrentIndex(1);
	else if(Settings::LANGUAGE == QLatin1String("ru"))
		ui->lang->setCurrentIndex(2);
	else
		ui->lang->setCurrentIndex(0);

	connect(ui->lang, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int index) {
		switch(index)
		{
		case 1: emit langChanged(QStringLiteral("en")); break;
		case 2: emit langChanged(QStringLiteral("ru")); break;
		default: emit langChanged(QStringLiteral("et")); break;
		}
		ui->retranslateUi(this);
		loadImages();
	});
	ui->continueBtn->setFont(buttonFont);

	ui->coatOfArms->load(QStringLiteral(":/images/Logo_Suur.svg"));
	ui->leaves->load(QStringLiteral(":/images/vapilehed.svg"));
	ui->structureFunds->load(QStringLiteral(":/images/Struktuurifondid.svg"));

	// Page 2: intro
	ui->introTitle->setFont(titleFont);
	ui->introLabelSign->setFont(dmLabelFont);
	ui->introLabelCrypto->setFont(dmLabelFont);
	ui->introLabelEid->setFont(dmLabelFont);
	ui->introIntroSign->setFont(regular14);
	ui->introIntroCrypto->setFont(regular14);
	ui->introIntroEid->setFont(regular14);
	ui->introSkip->setFont(regular12);
	ui->introViewSigning->setFont(buttonFont);
	ui->introViewEncryption->setFont(buttonFont);
	ui->introViewEid->setFont(buttonFont);
	ui->signWidget->load(QStringLiteral(":/images/icon_Allkiri_hover.svg"));
	ui->cryptoWidget->load(QStringLiteral(":/images/icon_Krypto_hover.svg"));
	ui->eidWidget->load(QStringLiteral(":/images/icon_Minu_eID_hover.svg"));

	// Page 3: Signing
	ui->signTitle->setFont(titleFont);
	ui->signLabel1->setFont(dmLabelFont);
	ui->signLabel2->setFont(dmLabelFont);
	ui->signLabel3->setFont(dmLabelFont);
	ui->signText1->setFont(regular14);
	ui->signText2->setFont(regular14);
	ui->signText3->setFont(regular14);
	ui->signSkip->setFont(regular12);
	ui->signNext->setFont(buttonFont);
	ui->signOne->load(QStringLiteral(":/images/intro_one.svg"));
	ui->signTwo->load(QStringLiteral(":/images/intro_two.svg"));
	ui->signThree->load(QStringLiteral(":/images/intro_three.svg"));

	// Page 4: Crypto
	ui->cryptoTitle->setFont(titleFont);
	ui->cryptoLabel1->setFont(dmLabelFont);
	ui->cryptoLabel2->setFont(dmLabelFont);
	ui->cryptoLabel3->setFont(dmLabelFont);
	ui->cryptoText1->setFont(regular14);
	ui->cryptoText2->setFont(regular14);
	ui->cryptoText3->setFont(regular14);
	ui->cryptoSkip->setFont(regular12);
	ui->cryptoNext->setFont(buttonFont);
	ui->cryptoOne->load(QStringLiteral(":/images/intro_one.svg"));
	ui->cryptoTwo->load(QStringLiteral(":/images/intro_two.svg"));
	ui->cryptoThree->load(QStringLiteral(":/images/intro_three.svg"));

	// Page 5: My eID
	ui->eidTitle->setFont(titleFont);
	ui->eidLabel1->setFont(dmLabelFont);
	ui->eidLabel3->setFont(dmLabelFont);
	ui->eidText1->setFont(regular14);
	ui->eidText3->setFont(regular14);
	ui->eidEnter->setFont(buttonFont);

	connect(ui->buttonGroup, QOverload<QAbstractButton*>::of(&QButtonGroup::buttonClicked), this, [this](QAbstractButton *b){
		if(b == ui->continueBtn)
			ui->stack->setCurrentIndex(Intro);
		if(b == ui->introViewSigning ||
			b == ui->cryptoGotoSigning ||
			b == ui->eidGotoSigning)
			ui->stack->setCurrentIndex(Signing);
		if(b == ui->introViewEncryption ||
			b == ui->signNext ||
			b == ui->signGotoEncryption ||
			b == ui->eidGotoEncryption)
			ui->stack->setCurrentIndex(Encryption);
		if(b == ui->introViewEid ||
			b == ui->cryptoNext ||
			b == ui->signGotoEid ||
			b == ui->cryptoGotoEid)
			ui->stack->setCurrentIndex(MyEid);
		if(b == ui->introSkip ||
			b == ui->signSkip ||
			b == ui->cryptoSkip ||
			b == ui->eidEnter)
			close();
	});

	loadImages();
}

FirstRun::~FirstRun()
{
	delete ui;
}

void FirstRun::keyPressEvent(QKeyEvent *event)
{
	auto next = ui->stack->currentIndex();
	switch(event->key())
	{
	case Qt::Key_Left:
		next = std::max<decltype(next)>(ui->stack->currentIndex() - 1, Language);
		break;
	case Qt::Key_Right:
		next = std::min<decltype(next)>(ui->stack->currentIndex() + 1, MyEid);
		break;
	default:
		QDialog::keyPressEvent(event);
		break;
	}

	if(next != ui->stack->currentIndex())
		ui->stack->setCurrentIndex(next);
}

void FirstRun::loadImages()
{
	QString lang = Settings::LANGUAGE;
	auto loadPixmap = [lang](const QString &base, QLabel *label) {
		label->setPixmap(QPixmap(QStringLiteral(":/images/%1_%2.png").arg(base, lang)));
	};
	loadPixmap(QStringLiteral("intro_sign-select"), ui->signImage1);
	loadPixmap(QStringLiteral("intro_sign-sign"), ui->signImage2);
	loadPixmap(QStringLiteral("intro_sign-pin"), ui->signImage3);
	loadPixmap(QStringLiteral("intro_crypto-select"), ui->cryptoImage1);
	loadPixmap(QStringLiteral("intro_crypto-recipient"), ui->cryptoImage2);
	loadPixmap(QStringLiteral("intro_crypto-encrypt"), ui->cryptoImage3);
	loadPixmap(QStringLiteral("intro_eid-manage"), ui->eidImage1);
	loadPixmap(QStringLiteral("intro_eid-info"), ui->eidImage3);
}
