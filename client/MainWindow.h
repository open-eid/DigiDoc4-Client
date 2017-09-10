/*
 * QDigiDoc4
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include "widgets/PageIcon.h"
#include "widgets/AccordionTitle.h"

#include <QWidget>
#include <QButtonGroup>
#include <QMessageBox>

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
    void buttonClicked( int button );

private:
    enum MainActions {
        HeadSettings,
        HeadHelp
	};

    enum Pages {
        SignIntro,
        SignDetails,
        CryptoIntro,
        CryptoDetails,
        MyEid
    };

    void navigateToPage( Pages page );
    void onAction( const QString &action );
    
    Ui::MainWindow *ui;

   	QButtonGroup *buttonGroup = nullptr;
};

#endif // MAINWINDOW_H
