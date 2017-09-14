#include "OtherData.h"
#include "ui_OtherData.h"

OtherData::OtherData(QWidget *parent) :
	QWidget(parent),
	ui(new Ui::OtherData)
{
	ui->setupUi(this);

	connect(ui->inputEMail, &QLineEdit::textChanged, this, [this](){ update(true); });
}

OtherData::~OtherData()
{
	delete ui;
}

void OtherData::update(bool activate, const QString &eMail)
{
	if(!eMail.isEmpty())
	{
		ui->btnCheckEMail->setVisible(false);
		ui->lblEMail->setVisible(true);
		ui->activateEMail->setVisible(false);

		ui->lblEMail->setText(QString("Teie @eesti.ee posti aadressid on suunatud e-postile <b>") + eMail + QString("</b>"));
	}
	else if(activate)
	{
		ui->btnCheckEMail->setVisible(false);
		ui->lblEMail->setVisible(false);
		ui->activateEMail->setVisible(true);

		if(ui->inputEMail->text().isEmpty())
		{
			ui->activate->setStyleSheet(
						"border-radius: 3px;"
						"background-color: #80aed5;"
						"font-family: Open Sans;"
						"font-size: 13px;"
						"color: #ffffff;"
						"font-weight: 400;"
						);
		}
		else
		{
			ui->activate->setStyleSheet(
						"border-radius: 3px;"
						"background-color: #006eb5;"
						"font-family: Open Sans;"
						"font-size: 13px;"
						"color: #ffffff;"
						"font-weight: 400;"
						);
		}
	}
	else
	{
		ui->btnCheckEMail->setVisible(true);
		ui->lblEMail->setVisible(false);
		ui->activateEMail->setVisible(false);
	}
}

void OtherData::paintEvent(QPaintEvent *)
{
	QStyleOption opt;
	opt.init(this);
	QPainter p(this);
	style()->drawPrimitive(QStyle::PE_Widget, &opt, &p, this);
}
