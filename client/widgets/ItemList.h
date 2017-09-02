#ifndef ITEMLIST_H
#define ITEMLIST_H

#include <QWidget>

namespace Ui {
class ItemList;
}

class ItemList : public QWidget
{
    Q_OBJECT

public:
    explicit ItemList(QWidget *parent = 0);
    ~ItemList();

private:
    Ui::ItemList *ui;
};

#endif // ITEMLIST_H
