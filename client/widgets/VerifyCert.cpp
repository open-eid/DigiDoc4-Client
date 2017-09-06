#include "VerifyCert.h"
#include "ui_VerifyCert.h"

VerifyCert::VerifyCert(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::VerifyCert)
{
    ui->setupUi(this);
}

VerifyCert::~VerifyCert()
{
    delete ui;
}

void VerifyCert::update(bool isValid, bool isPUK, const QString &name, const QString &validUntil, const QString &change, const QString &error )
{
    if(isValid)
    {
        setStyleSheet("background-color: #ffffff;");
        ui->OK_icon->setStyleSheet(
                    "border: none;"
                    "image: url(:/images/ok.png);"
                    );
        ui->changePIN->setStyleSheet(
                    "width: 157px;"
                    "height: 30px;"
                    "padding: 6px 10px;"
                    "border-radius: 3px;"
                    "background-color: #4a82f3;"
                    "font-family: Open Sans;"
                    "font-size: 14px;"
                    "color: #ffffff;"
                    "font-weight: 400;"
                    "line-height: 19px;"
                    "text-align: center;"
                    );
    }
    else
    {
        setStyleSheet("border: 2px solid #e89c30;"
                      "background-color: #fcf5ea;" );
        ui->OK_icon->setStyleSheet(
                    "border: none;"
                    "image: url(:/images/alert.png);"
                    );
        ui->changePIN->setStyleSheet(
                    "border: none;"
                    "width: 157px;"
                    "height: 30px;"
                    "padding: 6px 10px;"
                    "border-radius: 3px;"
                    "background-color: #53c964;"
                    "font-family: Open Sans;"
                    "font-size: 14px;"
                    "color: #ffffff;"
                    "font-weight: 400;"
                    "line-height: 19px;"
                    "text-align: center;"
                    );
    }

    ui->name->setText(name);
    ui->validUntil->setText(validUntil);
    ui->error->setVisible(!isValid);
    ui->error->setText(error);
    ui->changePIN->setText(change);

    ui->forgot_PIN->setVisible(!isPUK);
    ui->details->setVisible(!isPUK);
}

// Needed to setStyleSheet() take effect.
void VerifyCert::paintEvent(QPaintEvent *)
{
    QStyleOption opt;
    opt.init(this);
    QPainter p(this);
    style()->drawPrimitive(QStyle::PE_Widget, &opt, &p, this);
}
