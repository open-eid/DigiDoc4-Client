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

	//	Page 1: language
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

	ui->coatOfArms->load(QStringLiteral(":/images/Logo_Suur.svg"));
	ui->leaves->load(QStringLiteral(":/images/vapilehed.svg"));
	ui->structureFunds->load(QStringLiteral(":/images/Struktuurifondid.svg"));
	ui->signWidget->load(QStringLiteral(":/images/icon_Allkiri_hover.svg"));
	ui->cryptoWidget->load(QStringLiteral(":/images/icon_Krypto_hover.svg"));
	ui->eidWidget->load(QStringLiteral(":/images/icon_Minu_eID_hover.svg"));
	ui->signOne->load(QStringLiteral(":/images/intro_one.svg"));
	ui->signTwo->load(QStringLiteral(":/images/intro_two.svg"));
	ui->signThree->load(QStringLiteral(":/images/intro_three.svg"));
	ui->cryptoOne->load(QStringLiteral(":/images/intro_one.svg"));
	ui->cryptoTwo->load(QStringLiteral(":/images/intro_two.svg"));
	ui->cryptoThree->load(QStringLiteral(":/images/intro_three.svg"));

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
