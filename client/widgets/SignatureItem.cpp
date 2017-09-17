#include "SignatureItem.h"
#include "ui_SignatureItem.h"
#include "Styles.h"

using namespace ria::qdigidoc4;

SignatureItem::SignatureItem(ContainerState state, QWidget *parent)
: StyledWidget(parent)
, ui(new Ui::SignatureItem)
{
	ui->setupUi(this);
	ui->name->setFont( Styles::font( Styles::Regular, 14, QFont::DemiBold ) );
	ui->status->setFont( Styles::font(Styles::Regular, 14) );
	ui->idSignTime->setFont( Styles::font(Styles::Regular, 11) );
	// ui->signatureInfo->setTextFormat( Qt::RichText );
	//ui->signatureInfo->setText( "<span style=\"font:1000\">MARI MAASIKAS MUSTIKAS</span>, 4840505123 - <span style=\"color:#53c964\">Allkiri on kehtiv</span><br/>"\
	//"<span style=\"font-size:12px\">Allkirjastas 12. september 2017, kell 13:22</span>" );
}

SignatureItem::~SignatureItem()
{
	delete ui;
}

void SignatureItem::stateChange(ContainerState state)
{
}
