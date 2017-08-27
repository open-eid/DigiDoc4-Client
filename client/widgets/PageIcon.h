#ifndef PAGEICON_H
#define PAGEICON_H

#include <QFont>
#include <QPaintEvent>
#include <QString>
#include <QWidget>

namespace Ui {
class PageIcon;
}

class PageIcon : public QWidget
{
    Q_OBJECT

public:
    struct Style
    {
        QFont font;
        QString image, backColor, foreColor;
    };
    explicit PageIcon(QWidget *parent = 0);
    ~PageIcon();

    void init(const QString &label, const Style &active, const Style &inactive, bool selected);

protected:
    virtual void mouseReleaseEvent(QMouseEvent *event);
    virtual void paintEvent(QPaintEvent *ev);

private:
    Ui::PageIcon *ui;
    Style active;
    Style inactive;
    bool selected;

    void updateSelection();
};

#endif // PAGEICON_H
