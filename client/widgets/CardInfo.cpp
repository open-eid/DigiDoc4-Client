#include "CardInfo.h"
#include "ui_CardInfo.h"

CardInfo::CardInfo(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::CardInfo)
{
    ui->setupUi(this);
}

CardInfo::~CardInfo()
{
    delete ui;
}

void CardInfo::fonts(const QFont &regular, const QFont &semiBold)
{
    ui->cardName->setFont(semiBold);
    ui->cardCode->setFont(regular);
    ui->cardStatus->setFont(regular);
}

void CardInfo::update(const QString &name, const QString &code, const QString &status)
{
    ui->cardName->setText(name);
    ui->cardCode->setText(code);
    ui->cardStatus->setText(status);
}