#include "Accordion.h"
#include "ui_Accordion.h"

Accordion::Accordion(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::Accordion),
    openSection(nullptr)
{
    ui->setupUi(this);
}

Accordion::~Accordion()
{
    delete ui;
}

void Accordion::init()
{
    // Initialize accordion.
    openSection = ui->titleVerirfyCert;

    ui->titleOther_EID  ->init(this, false, "teised eIDd", ui->contentOther_EID );
    ui->titleVerirfyCert->init(this, true, "PIN/PUK koodid ja sertifikaatide kontroll", ui->contentVerifyCert );
    ui->titleOtherData  ->init(this, false, "Muud andmed", ui->contentOtherData );

    // Initialize PIN/PUK content widgets.
    ui->IdentCert->update(true,  false, "Isikutuvastamise sertifikaat", "Sertifikaat kehtib kuni 10. veebruar 2019", "Muuda PIN1");
    ui->SignCert ->update(false, false, "Allkirjastamise sertifikaat",  "Sertifikaat on aegunud!",                   "Muuda PIN2", "PIN2 on blokeeritud, kuna PIN2 koodi on sisestatud 3 korda valesti. Tühista blokeering, et PIN2 taas kasutada.");
    ui->PUK_Code ->update(true,  true,  "PUK kood",                     "PUK kood asub Teie kooodiümbrikus",         "Muuda PUK");
}

void Accordion::closeOtherSection(AccordionTitle* opened)
{
    openSection->closeSection();
    openSection = opened;
}



// Needed to setStyleSheet() take effect.
void Accordion::paintEvent(QPaintEvent *)
{
    QStyleOption opt;
    opt.init(this);
    QPainter p(this);
    style()->drawPrimitive(QStyle::PE_Widget, &opt, &p, this);
}
