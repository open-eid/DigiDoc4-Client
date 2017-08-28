#ifndef CARDINFO_H
#define CARDINFO_H

#include <QFont>
#include <QWidget>

namespace Ui {
class CardInfo;
}

class CardInfo : public QWidget
{
    Q_OBJECT

public:
    explicit CardInfo(QWidget *parent = 0);
    ~CardInfo();

    void fonts(const QFont &regular, const QFont &semiBold);
    void update(const QString &name, const QString &code, const QString &status);

private:
    Ui::CardInfo *ui;
};

#endif // CARDINFO_H
