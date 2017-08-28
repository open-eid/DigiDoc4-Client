#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include "widgets/PageIcon.h"

#include <QWidget>

namespace Ui {
class MainWindow;
}

class MainWindow : public QWidget
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = 0);
    ~MainWindow();

private Q_SLOTS:
	void pageSelected( PageIcon *const );

private:
    Ui::MainWindow *ui;
};

#endif // MAINWINDOW_H
