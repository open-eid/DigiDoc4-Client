#include "InfoStack.h"
#include "ui_InfoStack.h"

InfoStack::InfoStack(QWidget *parent) :
	QWidget(parent),
	ui(new Ui::InfoStack)
{
	ui->setupUi(this);
	ui->verifyCert->hide();
}

InfoStack::~InfoStack()
{
	delete ui;
}

void InfoStack::clearPicture()
{
	ui->photo->clear();
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

// Needed to setStyleSheet() take effect.
void InfoStack::paintEvent(QPaintEvent *)
{
	QStyleOption opt;
	opt.init(this);
	QPainter p(this);
	style()->drawPrimitive(QStyle::PE_Widget, &opt, &p, this);
}
