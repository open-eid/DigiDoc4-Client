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
    explicit SignatureItem(ContainerState state, QWidget *parent = 0);
    ~SignatureItem();

    void stateChange(ContainerState state) override;
    
private:
    Ui::SignatureItem *ui;
};

#endif // SIGNATUREITEM_H
