#include "Accordion.h"
#include "ui_Accordion.h"

Accordion::Accordion(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::Accordion)
{
    ui->setupUi(this);
}

Accordion::~Accordion()
{
    delete ui;
}

void Accordion::init()
{
    ui->titleOther_EID->init( "teised eIDd", ui->contentOther_EID );
    ui->titleVerirfyCert->init( "PIN/PUK koodid ja sertifikaatide kontroll", ui->contentVerifyCert );
    ui->titleOtherData->init( "Muud andmed", ui->contentOtherData );
}


// Needed to setStyleSheet() take effect.
void Accordion::paintEvent(QPaintEvent *)
{
    QStyleOption opt;
    opt.init(this);
    QPainter p(this);
    style()->drawPrimitive(QStyle::PE_Widget, &opt, &p, this);
}
