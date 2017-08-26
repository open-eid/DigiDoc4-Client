#ifndef MAINWINDOW3_H
#define MAINWINDOW3_H

#include <QWidget>

namespace Ui {
class MainWindow3;
}

class MainWindow3 : public QWidget
{
    Q_OBJECT

public:
    explicit MainWindow3(QWidget *parent = 0);
    ~MainWindow3();

private:
    Ui::MainWindow3 *ui;
};

#endif // MAINWINDOW3_H
