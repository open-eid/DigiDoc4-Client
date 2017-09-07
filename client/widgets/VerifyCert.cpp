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

void VerifyCert::update(bool isValid, const QString &name, const QString &validUntil, const QString &change, const QString &forgot_PIN_HTML, const QString &details_HTML, const QString &error)
{
    if(isValid)
    {
        this->setStyleSheet(
                    "border: 1px solid #afafaf;"
                    "background-color: #ffffff;"
                    );
        ui->OK_icon->setStyleSheet(
                    "border: none;"
                    "image: url(:/images/ok.png);"
                    );
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
        ui->OK_icon->setStyleSheet(
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

    if(forgot_PIN_HTML.isEmpty())
        ui->forgot_PIN->setVisible(false);
    else
    {
        ui->forgot_PIN->setVisible(true);
        ui->forgot_PIN->setText(forgot_PIN_HTML);
        ui->forgot_PIN->setOpenExternalLinks(true);
    }

    if(details_HTML.isEmpty())
        ui->details->setVisible(false);
    else
    {
        ui->details->setText(details_HTML);
        ui->details->setVisible(true);
        ui->details->setOpenExternalLinks(true);
    }
}

// Needed to setStyleSheet() take effect.
void VerifyCert::paintEvent(QPaintEvent *)
{
    QStyleOption opt;
    opt.init(this);
    QPainter p(this);
    style()->drawPrimitive(QStyle::PE_Widget, &opt, &p, this);
}
