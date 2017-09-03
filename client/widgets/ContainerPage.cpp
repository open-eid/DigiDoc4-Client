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

 #include "ContainerPage.h"
#include "ui_ContainerPage.h"

ContainerPage::ContainerPage(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::ContainerPage)
{
    ui->setupUi(this);
    init();

    connect( ui->mainAction, &LabelButton::linkActivated, this, &ContainerPage::action );
}

ContainerPage::~ContainerPage()
{
    delete ui;
}

void ContainerPage::init()
{
    QFont semiBold("OpenSans-Semibold", 13);
#ifdef Q_OS_MAC
    semiBold.setWeight(QFont::DemiBold);
#endif
    QFont regular(QFont("OpenSans-Regular", 13));
    ui->container->setFont(semiBold);
    ui->containerFile->setFont(regular);
    ui->changeLocation->init("Muuda", "#container-location", "#006eb5", "#ffffff");
    ui->changeLocation->setFont(regular);
    ui->cancel->init("Katkesta", "#cancel", "#c53e3e", "#f7f7f7");
    ui->mainAction->init("Allkirjasta ID-Kaardiga", "#mainAction", "#f7f7f7", "#6edc6c");
}

void ContainerPage::hideRightPane()
{
    ui->rightPane->hide();
}

void ContainerPage::showRightPane()
{
    ui->rightPane->show();
}
