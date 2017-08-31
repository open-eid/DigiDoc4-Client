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

#include "CardInfo.h"
#include "ui_CardInfo.h"

CardInfo::CardInfo(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::CardInfo)
{
    ui->setupUi(this);
    QFont openSansReg14("OpenSans-Regular", 14);
    QFont openSansSBold14("OpenSans-SemiBold", 14);
    
    ui->cardName->setFont(openSansSBold14);
    ui->cardCode->setFont(openSansReg14);
    ui->cardStatus->setFont(openSansReg14);
}

CardInfo::~CardInfo()
{
    delete ui;
}

void CardInfo::update(const QString &name, const QString &code, const QString &status)
{
    ui->cardName->setText(name);
    ui->cardCode->setText(code);
    ui->cardStatus->setText(status);
}
