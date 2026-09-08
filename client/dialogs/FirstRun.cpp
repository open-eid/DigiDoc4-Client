// SPDX-FileCopyrightText: Estonian Information System Authority
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "FirstRun.h"
#include "ui_FirstRun.h"

#include "Settings.h"

#include <QApplication>
#include <QPixmap>

FirstRun::FirstRun(QWidget *parent)
	: QDialog(parent)
	, ui(new Ui::FirstRun)
{
	ui->setupUi(this);
	setWindowFlag(Qt::FramelessWindowHint);
	setAttribute(Qt::WA_DeleteOnClose);
	setFixedSize(size());
	if(parent)
		move(parent->geometry().center() - geometry().center());

	ui->langEn->setChecked(Settings::LANGUAGE == QLatin1String("en"));
	ui->langEt->setChecked(!ui->langEn->isChecked());
	ui->pageGroup->setId(ui->navIntro, Intro);
	ui->pageGroup->setId(ui->navSign, Signing);
	ui->pageGroup->setId(ui->navCrypto, Encryption);
	ui->pageGroup->setId(ui->navEid, MyEid);
	connect(ui->langGroup, qOverload<QAbstractButton*>(&QButtonGroup::buttonClicked), this,
		[this](QAbstractButton *button) {
		emit langChanged(button == ui->langEn ? QStringLiteral("en") : QStringLiteral("et"));
		ui->retranslateUi(this);
		loadImages();
		setView(ui->pageGroup->button(ui->stack->currentIndex()));
	});

	ui->coatOfArms->load(QStringLiteral(":/images/Logo_Suur.svg"));
	ui->leaves->load(QStringLiteral(":/images/vapilehed.svg"));
	ui->structureFunds->load(QStringLiteral(":/images/Struktuurifondid.svg"));
	ui->signWidget->load(QStringLiteral(":/images/intro_sign.svg"));
	ui->cryptoWidget->load(QStringLiteral(":/images/intro_crypto.svg"));
	ui->eidWidget->load(QStringLiteral(":/images/intro_myeid.svg"));
	connect(ui->pageGroup, qOverload<QAbstractButton*>(&QButtonGroup::buttonClicked),
		this, &FirstRun::setView);
	connect(ui->btnSkip, &QPushButton::clicked, this, &FirstRun::close);
	connect(ui->btnNext, &QPushButton::clicked, this, [this] {
		if(ui->stack->currentIndex() == MyEid)
			close();
		else
			setView(ui->pageGroup->button(ui->stack->currentIndex() + 1));
	});

	loadImages();
	setView(nullptr);
}

FirstRun::~FirstRun()
{
	delete ui;
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

void FirstRun::setView(QAbstractButton *button)
{
	const auto view = button ? View(ui->pageGroup->id(button)) : Language;
	ui->stack->setCurrentIndex(view);
	ui->navPane->setVisible(view != Language);
	if(button)
		button->setChecked(true);

	ui->btnNext->setText(view == MyEid ? tr("Enter the application") :
		view == Language ? tr("Continue") : tr("View next intro"));
	ui->btnSkip->setText(tr("Close introduction"));
	ui->txtNavVersion->setText(tr("DigiDoc4 version %1, released %2")
		.arg(QApplication::applicationVersion(), QStringLiteral(BUILD_DATE)));
}
