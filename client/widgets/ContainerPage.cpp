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
#include "widgets/ContainerItem.h"

using namespace ria::qdigidoc4;

ContainerPage::ContainerPage(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::ContainerPage)
{
    ui->setupUi(this);
    init();
}

ContainerPage::~ContainerPage()
{
    delete ui;
}

void ContainerPage::init()
{
    static const QString style = "QLabel { padding: 6px 10px; border-top-left-radius: 3px; border-bottom-left-radius: 3px; background-color: %1; color: #ffffff; border: none; text-decoration: none solid; }";
    static const QString link = "<a href=\"#mainAction\" style=\"background-color: %1; color: #ffffff; text-decoration: none solid;\">Allkirjasta ID-Kaardiga</a>";
    
    ui->leftPane->init(ItemList::File, "Kontaineri failid");

    QFont semiBold = Styles::instance().font(Styles::OpenSansSemiBold, 13);
    QFont regular = Styles::instance().font(Styles::OpenSansRegular, 13);
    ui->container->setFont(semiBold);
    ui->containerFile->setFont(regular);
    ui->changeLocation->init(LabelButton::DeepCerulean | LabelButton::WhiteBackground, "Muuda", Actions::ContainerLocation);
    ui->cancel->init(LabelButton::Mojo, "Katkesta", Actions::ContainerCancel);
    ui->encrypt->init(LabelButton::DeepCerulean, "Krüpteeri", Actions::ContainerEncrypt);
    ui->navigateToContainer->init(LabelButton::DeepCerulean, "Ava konteineri asukoht", Actions::ContainerNavigate);
    ui->email->init(LabelButton::DeepCerulean, "Edasta e-mailiga", Actions::ContainerEmail);
    ui->save->init(LabelButton::DeepCerulean, "Salvesta allkirjastamata", Actions::ContainerSave);
    ui->mainAction->setStyles(style.arg("#6edc6c"), link.arg("#6edc6c"), style.arg("#53c964"), link.arg("#53c964"));

    connect( ui->mainAction, &LabelButton::clicked, this, &ContainerPage::action );
}

void ContainerPage::transition(ContainerState state)
{
    ui->leftPane->stateChange(state);
    ui->rightPane->stateChange(state);

    switch( state )
    {
    case UnsignedContainer:
        hideRightPane();
        ui->leftPane->init(ItemList::File, "Allkirjastamiseks valitud failid");
        ui->mainAction->init( SignatureAdd );
        for(LabelButton *button: {ui->cancel, ui->save, ui->mainAction })
        {
            button->show();
        }
        for(LabelButton *button: {ui->encrypt, ui->navigateToContainer, ui->email })
        {
            button->hide();
        }
        break;
    case UnsignedSavedContainer:
        break;
    case SignedContainer:
        ui->leftPane->init(ItemList::File, "Kontaineri failid");
        showRightPane(ItemList::Signature, "Kontaineri allkirjad");
        ui->rightPane->add(SignatureAdd);
        for(LabelButton *button: {ui->cancel, ui->save, ui->mainAction })
        {
            button->hide();
        }
        for(LabelButton *button: {ui->encrypt, ui->navigateToContainer, ui->email })
        {
            button->show();
        }
        break;

    case UnencryptedContainer:
        break;
    case EncryptedContainer:
        break;
    case DecryptedContainer:
        break;
    }
}

void ContainerPage::hideRightPane()
{
    ui->rightPane->hide();
}

void ContainerPage::showRightPane(ItemList::ItemType itemType, const QString &header)
{
    ui->rightPane->init(itemType, header);
    ui->rightPane->show();
}
