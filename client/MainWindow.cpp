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

#include "MainWindow.h"
#include "ui_MainWindow.h"
#include "Styles.h"

#include <QDebug>

MainWindow::MainWindow(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    qDebug() << "Set fonts";
    const QFont& regular = Styles::instance().fontRegular();
    const QFont& semiBold = Styles::instance().fontSemiBold();
    ui->signature->init( "ALLKIRI", PageIcon::Style { semiBold, "/images/sign_dark_38x38.png", "#ffffff", "#998B66" },
        PageIcon::Style { regular, "/images/sign_light_38x38.png", "#023664", "#ffffff" }, true );
    ui->crypto->init( "KRÜPTO", PageIcon::Style { semiBold, "/images/crypto_dark_38x38.png", "#ffffff", "#998B66" },
        PageIcon::Style { regular, "/images/crypto_light_38x38.png", "#023664", "#ffffff" }, false );
    ui->myEid->init("MINU eID", PageIcon::Style { semiBold, "/images/my_eid_dark_38x38.png", "#ffffff", "#998B66" },
        PageIcon::Style { regular, "/images/my_eid_light_38x38.png", "#023664", "#ffffff" }, false );

    ui->cardInfo->fonts(regular, semiBold);
    ui->cardInfo->update("MARI MAASIKAS", "4845050123", "Lugejas on ID kaart");

    connect( ui->signature, SIGNAL(activated( PageIcon *const )), SLOT(pageSelected( PageIcon *const )) );
    connect( ui->crypto, SIGNAL(activated( PageIcon *const )), SLOT(pageSelected( PageIcon *const )) );
    connect( ui->myEid, SIGNAL(activated( PageIcon *const )), SLOT(pageSelected( PageIcon *const )) );
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::pageSelected( PageIcon *const page )
{
    if( page != ui->signature ) {
        ui->signature->select(false);
    } else {
        ui->startScreen->setCurrentIndex(0);
        ui->signaturePageLabel->setFont(Styles::instance().fontRegular());
    }
    if( page != ui->crypto ) {
        ui->crypto->select(false);
    } else {
        ui->startScreen->setCurrentIndex(1);
    }
    if( page != ui->myEid ) {
        ui->myEid->select(false);
    } else {
        ui->startScreen->setCurrentIndex(2);
    }
}
