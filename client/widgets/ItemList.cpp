#include "ItemList.h"
#include "ui_ItemList.h"

ItemList::ItemList(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::ItemList)
{
    ui->setupUi(this);
    ui->add->init("+ Lisa veel faile", "add-file", "#006eb5", "#ffffff");    
}

ItemList::~ItemList()
{
    delete ui;
}
