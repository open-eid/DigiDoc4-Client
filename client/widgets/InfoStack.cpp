#include "InfoStack.h"
#include "ui_InfoStack.h"

InfoStack::InfoStack(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::InfoStack)
{
    ui->setupUi(this);
}

InfoStack::~InfoStack()
{
    delete ui;
}

void InfoStack::update(const QString &givenNames, const QString &surname, const QString &personalCode, const QString &citizenship, const QString &serialNumber, const QString &expiryDate, const QString &verifyCert )
{
    ui->valueGivenNames->setText(givenNames);
    ui->valueSurname->setText(surname);
    ui->valuePersonalCode->setText(personalCode);
    ui->valueCitizenship->setText(citizenship);
    ui->valueSerialNumber->setText(serialNumber);
    ui->valueExpiryDate->setText(expiryDate);
    ui->verifyCert->setText(verifyCert);
}


// Needed to setStyleSheet() take effect.
void InfoStack::paintEvent(QPaintEvent *)
{
    QStyleOption opt;
    opt.init(this);
    QPainter p(this);
    style()->drawPrimitive(QStyle::PE_Widget, &opt, &p, this);
}
