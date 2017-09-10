#ifndef SIGNATUREITEM_H
#define SIGNATUREITEM_H

#include "widgets/ContainerItem.h"

namespace Ui {
class SignatureItem;
}

class SignatureItem : public ContainerItem
{
    Q_OBJECT

public:
    explicit SignatureItem(ria::qdigidoc4::ContainerState state, QWidget *parent = 0);
    ~SignatureItem();

    void stateChange(ria::qdigidoc4::ContainerState state) override;
    
private:
    Ui::SignatureItem *ui;
};

#endif // SIGNATUREITEM_H
