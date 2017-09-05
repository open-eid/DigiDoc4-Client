#ifndef ACCORDIONTITLE_H
#define ACCORDIONTITLE_H

#include <QWidget>
#include <QPainter>

namespace Ui {
class AccordionTitle;
}

class AccordionTitle : public QWidget
{
    Q_OBJECT

public:
    explicit AccordionTitle(QWidget *parent = 0);
    ~AccordionTitle();

    void init(const QString& caption, QWidget* content);

protected:
    void mouseReleaseEvent(QMouseEvent *event) override;
    void paintEvent(QPaintEvent *) override;

private:
    Ui::AccordionTitle *ui;

    QWidget* content;
};

#endif // ACCORDIONTITLE_H
