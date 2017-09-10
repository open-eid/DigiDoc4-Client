/*
 * QDigiDoc4
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#ifndef ITEMLIST_H
#define ITEMLIST_H

#include "common_enums.h"
#include "widgets/ItemWidget.h"

#include <QWidget>

namespace Ui {
class ItemList;
}

class ItemList : public QWidget
{
    Q_OBJECT

public:
    enum ItemType {
        File,
        Signature,
        Address
    };

    explicit ItemList(QWidget *parent = 0);
    virtual ~ItemList();

    void init(ItemType itemType, const QString &header);
    void add(int code);
    void stateChange(ria::qdigidoc4::ContainerState state);

private:
    QString addLabel() const;
    QString anchor() const;

    Ui::ItemList* ui;
    ria::qdigidoc4::ContainerState state;
    ItemType itemType;
    std::vector<ItemWidget*> items;
};

#endif // ITEMLIST_H
