#include "ItemList.h"
#include "ui_ItemList.h"

#include <vector>

ItemList::ItemList(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::ItemList)
{
    ui->setupUi(this);
    ui->add->init("+ Lisa veel faile", "add-file", "#006eb5", "#ffffff");    

    connect(ui->add, &QLabel::linkActivated, this, &ItemList::add);
}

ItemList::~ItemList()
{
    delete ui;
}

void ItemList::add(const QString &anchor)
{
    QWidget* item = new QWidget;
    item->setMinimumSize(460, 40);
    item->setStyleSheet("border: 1px solid #c8c8c8; background-color: #fafafa; color: #000000; text-decoration: none solid rgb(0, 0, 0);");
    item->show();
    ui->itemLayout->insertWidget(items.size(), item);
    items.push_back(item);
    // ui->itemLayout->addWidget(item);
}
