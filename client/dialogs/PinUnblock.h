#ifndef PINUNBLOCK_H
#define PINUNBLOCK_H

#include <common/PinDialog.h>

#include <QDialog>

namespace Ui {
class PinUnblock;
}

class PinUnblock : public QDialog
{
    Q_OBJECT

public:
    PinUnblock(QWidget *parent, PinDialog::PinFlags flags);
    ~PinUnblock();

private:
    void init(PinDialog::PinFlags flags);
    void setUnblockEnabled();

    Ui::PinUnblock *ui;
    QRegExp		regexpPUK;
    QRegExp		regexpPIN;
    bool isPukOk;
    bool isPinOk;
    bool isRepeatOk;
};

#endif // PINUNBLOCK_H
