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

#include "FileItem.h"
#include "ui_FileItem.h"
#include "Styles.h"

FileItem::FileItem(ContainerState state, QWidget *parent)
: ContainerItem(parent)
, ui(new Ui::FileItem)
{
    ui->setupUi(this);
    ui->fileName->setFont(Styles::instance().font(Styles::OpenSansRegular, 13));
    ui->remove->init(LabelButton::Mojo | LabelButton::AlabasterBackground, "Eemalda", "#remove-file");
    setStyleSheet("border: solid #c8c8c8; border-width: 1px 0px 1px 0px; background-color: #fafafa; color: #000000; text-decoration: none solid rgb(0, 0, 0);");
    stateChange(state);
}

FileItem::~FileItem()
{
    delete ui;
}

void FileItem::stateChange(ContainerState state)
{
    switch(state)
    {
    case UnsignedContainer:
    case UnsignedSavedContainer:
        ui->download->hide();
        ui->remove->show();
        break;
    case SignedContainer:
        ui->download->show();
        ui->remove->hide();
        break;
    
    case UnencryptedContainer:
        ui->download->hide();
        ui->remove->show();
        break;
    case EncryptedContainer:
        ui->download->hide();
        ui->remove->hide();
        break;
    case DecryptedContainer:
        ui->download->show();
        ui->remove->hide();
        break;
    }
}
