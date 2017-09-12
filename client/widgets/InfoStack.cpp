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

void InfoStack::showPicture( const QPixmap &pixmap )
{
    ui->photo->setProperty( "PICTURE", pixmap );
	ui->photo->setPixmap( pixmap.scaled( 108, 133, Qt::IgnoreAspectRatio, Qt::SmoothTransformation ) );
}
