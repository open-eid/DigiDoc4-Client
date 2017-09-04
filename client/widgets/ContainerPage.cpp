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
#include "Styles.h"

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
    static const QString style = "QLabel { padding: 6px 10px; border-top-left-radius: 3px; border-bottom-left-radius: 3px; background-color: %1; color: #ffffff; border: none; text-decoration: none solid; }";
    static const QString link = "<a href=\"#mainAction\" style=\"background-color: %1; color: #ffffff; text-decoration: none solid;\">Allkirjasta ID-Kaardiga</a>";
    
    QFont semiBold("OpenSans-Semibold", 13);
#ifdef Q_OS_MAC
    semiBold.setWeight(QFont::DemiBold);
#endif
    QFont regular = Styles::instance().font(Styles::OpenSansRegular, 13);
    ui->container->setFont(semiBold);
    ui->containerFile->setFont(regular);
    ui->changeLocation->init("Muuda", "#container-location", "#006eb5", "#ffffff");
    ui->changeLocation->setFont(regular);
    ui->cancel->init("Katkesta", "#cancel", "#c53e3e", "#f7f7f7");
    ui->mainAction->setStyles(style.arg("#6edc6c"), link.arg("#6edc6c"), style.arg("#53c964"), link.arg("#53c964"));
}

void ContainerPage::hideRightPane()
{
    ui->rightPane->hide();
}

void ContainerPage::showRightPane()
{
    ui->rightPane->show();
}
