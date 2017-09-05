#ifndef ACCORDION_H
#define ACCORDION_H

#include <QWidget>
#include <QPainter>

namespace Ui {
class Accordion;
}

class Accordion : public QWidget
{
    Q_OBJECT

public:
    explicit Accordion(QWidget *parent = 0);
    ~Accordion();

    void init();

protected:
    void paintEvent(QPaintEvent *) override;

private:
    Ui::Accordion *ui;
};

#endif // ACCORDION_H
