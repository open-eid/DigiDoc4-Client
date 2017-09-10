#pragma once

#include "widgets/ItemWidget.h"

namespace Ui {
class SignatureItem;
}

class SignatureItem : public ItemWidget
{
    Q_OBJECT

public:
    explicit SignatureItem(ria::qdigidoc4::ContainerState state, QWidget *parent = 0);
    ~SignatureItem();

    void stateChange(ria::qdigidoc4::ContainerState state) override;
    
private:
    Ui::SignatureItem *ui;
};

