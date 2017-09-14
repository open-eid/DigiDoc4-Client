#include "VerifyCert.h"
#include "ui_VerifyCert.h"

VerifyCert::VerifyCert(QWidget *parent) :
	QWidget(parent),
	ui(new Ui::VerifyCert),
	isValid(false)
{
	ui->setupUi(this);
}

VerifyCert::~VerifyCert()
{
	delete ui;
}

void VerifyCert::update(bool isValid, const QString &name, const QString &validUntil, const QString &change, const QString &forgotPinText, const QString &detailsText, const QString &error)
{
	this->isValid = isValid;

	if(isValid)
	{
		this->setStyleSheet("background-color: #ffffff;");
		ui->statusIcon->setStyleSheet("image: url(:/images/ok.png);");
		ui->changePIN->setStyleSheet(
					"border: 1px solid #4a82f3;"
					"padding: 6px 10px;"
					"border-radius: 3px;"
					"background-color: #4a82f3;"
					"font-family: Open Sans;"
					"font-size: 14px;"
					"color: #ffffff;"
					"font-weight: 400;"
					"text-align: center;"
					);
	}
	else
	{
		this->setStyleSheet(
					"border: 2px solid #e89c30;"
					"background-color: #fcf5ea;" );
		ui->statusIcon->setStyleSheet(
					"border: none;"
					"image: url(:/images/alert.png);"
					);
		ui->changePIN->setStyleSheet(
					"border: 1px solid #53c964;"
					"padding: 6px 10px;"
					"border-radius: 3px;"
					"background-color: #53c964;"
					"font-family: Open Sans;"
					"font-size: 14px;"
					"color: #ffffff;"
					"font-weight: 400;"
					"text-align: center;"
					);
	}

	ui->name->setText(name);
	ui->validUntil->setText(validUntil);
	ui->error->setVisible(!isValid);
	ui->error->setText(error);
	ui->changePIN->setText(change);

	if(forgotPinText.isEmpty())
	{
		ui->forgotPinLink->setVisible(false);
	}
	else
	{
		ui->forgotPinLink->setVisible(true);
		ui->forgotPinLink->setText(forgotPinText);
		ui->forgotPinLink->setOpenExternalLinks(true);
	}

	if(detailsText.isEmpty())
	{
		ui->details->setVisible(false);
	}
	else
	{
		ui->details->setText(detailsText);
		ui->details->setVisible(true);
		ui->details->setOpenExternalLinks(true);
	}
}

void VerifyCert::enterEvent(QEvent * event)
{
	if(isValid)
		this->setStyleSheet("background-color: #f7f7f7;");
}

void VerifyCert::leaveEvent(QEvent * event)
{
	if(isValid)
		this->setStyleSheet("background-color: white;");
}


// Needed to setStyleSheet() take effect.
void VerifyCert::paintEvent(QPaintEvent *)
{
	QStyleOption opt;
	opt.init(this);
	QPainter p(this);
	style()->drawPrimitive(QStyle::PE_Widget, &opt, &p, this);
}
