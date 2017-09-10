#include "SignatureItem.h"
#include "ui_SignatureItem.h"
#include "Styles.h"

using namespace ria::qdigidoc4;

SignatureItem::SignatureItem(ContainerState state, QWidget *parent)
: ItemWidget(parent)
, ui(new Ui::SignatureItem)
{
    ui->setupUi(this);
    ui->signatureInfo->setFont(Styles::instance().font(Styles::OpenSansRegular, 13));
    ui->remove->init(LabelButton::Mojo | LabelButton::AlabasterBackground, "Eemalda", SignatureRemove);
    setStyleSheet("border: solid #c8c8c8; border-width: 1px 0px 1px 0px; background-color: #fafafa; color: #000000; text-decoration: none solid rgb(0, 0, 0);");
}

SignatureItem::~SignatureItem()
{
    delete ui;
}

void SignatureItem::stateChange(ContainerState state)
{
}
