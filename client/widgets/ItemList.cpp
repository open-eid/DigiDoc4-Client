#include "ItemList.h"
#include "ui_ItemList.h"

ItemList::ItemList(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::ItemList)
{
    ui->setupUi(this);
}

ItemList::~ItemList()
{
    delete ui;
}
