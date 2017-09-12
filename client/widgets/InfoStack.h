#ifndef INFOSTACK_H
#define INFOSTACK_H

#include <QWidget>
#include <QPainter>


namespace Ui {
class InfoStack;
}

class InfoStack : public QWidget
{
    Q_OBJECT

public:
    explicit InfoStack(QWidget *parent = 0);
    ~InfoStack();

    void update(const QString &givenNames, const QString &surname, const QString &personalCode, const QString &citizenship, const QString &serialNumber, const QString &expiryDate, const QString &verifyCert );
    void showPicture( const QPixmap &pixmap );

private:
    Ui::InfoStack *ui;
};

#endif // INFOSTACK_H
