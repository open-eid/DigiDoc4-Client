#ifndef CONTAINERPAGE_H
#define CONTAINERPAGE_H

#include <QWidget>

namespace Ui {
class ContainerPage;
}

class ContainerPage : public QWidget
{
    Q_OBJECT

public:
    explicit ContainerPage(QWidget *parent = 0);
    ~ContainerPage();

private:
    Ui::ContainerPage *ui;
};

#endif // CONTAINERPAGE_H
