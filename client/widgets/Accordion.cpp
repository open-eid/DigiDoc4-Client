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
	openSection = ui->titleVerifyCert;

	ui->titleOtherEID->init(this,   false, "teised eIDd", ui->contentOtherEID );
	ui->titleVerifyCert->init(this, true,  "PIN/PUK koodid ja sertifikaatide kontroll", ui->contentVerifyCert );
	ui->titleOtherData->init(this,  false, "Muud andmed", ui->contentOtherData );

	// Initialize PIN/PUK content widgets.
	ui->authBox->update(true,  "Isikutuvastamise sertifikaat", "Sertifikaat kehtib kuni 10. veebruar 2019",  "Muuda PIN1",  "<a href='#pin1-forgotten'><span style='color:black;'>Unustasid PIN1</span></a>",  "<a href='#pin1-cert'><span style='color:black;'>Vaata sertifikaadi detaile</span></a>", "Kala");
	ui->signBox->update(false,  "Allkirjastamise sertifikaat",  "Sertifikaat on aegunud!",                    "Muuda PIN2",  "<a href='#pin2-forgotten'><span style='color:black;'>Unustasid PIN2</span></a>",  "<a href='#pin2-cert'><span style='color:black;'>Vaata sertifikaadi detaile</span></a>",  "PIN2 on blokeeritud, kuna PIN2 koodi on sisestatud 3 korda valesti. Tühista blokeering, et PIN2 taas kasutada.");
	ui->pukBox->update(true,    "PUK kood",                     "PUK kood asub Teie kooodiümbrikus",          "Muuda PUK");

	ui->contentOtherData->update(true);
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
