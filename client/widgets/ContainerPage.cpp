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
#include "dialogs/MobileDialog.h"

#include <QDir>
#include <QFileInfo>
#include <QGraphicsDropShadowEffect>

using namespace ria::qdigidoc4;


#define ACTION_WIDTH 200
#define ACTION_HEIGHT 65


ContainerPage::ContainerPage( QWidget *parent ) :
	QWidget( parent ),
	ui( new Ui::ContainerPage )
{
	ui->setupUi( this );
	init();
}

ContainerPage::~ContainerPage()
{
	delete ui;
}

void ContainerPage::init()
{
	ui->leftPane->init( ItemList::File, "Kontaineri failid" );

	QFont regular = Styles::font( Styles::Regular, 14 );
	ui->container->setFont( regular );
	ui->containerFile->setFont( regular );

	ui->changeLocation->setIcons( "/images/icon_Edit.svg", "/images/icon_Edit_hover.svg", "/images/icon_Edit_pressed.svg", 4, 4, 18, 18 );
	ui->changeLocation->init( LabelButton::BoxedDeepCeruleanWithCuriousBlue, "MUUDA", Actions::ContainerLocation );
	ui->cancel->init( LabelButton::BoxedMojo, "← KATKESTA", Actions::ContainerCancel );
	ui->encrypt->init( LabelButton::BoxedDeepCerulean, "KRÜPTEERI", Actions::ContainerEncrypt );
	ui->navigateToContainer->init( LabelButton::BoxedDeepCerulean, "AVA KONTAINERI ASUKOHT", Actions::ContainerNavigate );
	ui->email->init( LabelButton::BoxedDeepCerulean, "EDASTA E-MAILIGA", Actions::ContainerEmail );
	ui->save->init( LabelButton::BoxedDeepCerulean, "SALVESTA ALLKIRJASTAMATA", Actions::ContainerSave );

	connect( ui->cancel, &LabelButton::clicked, this, &ContainerPage::action );
	connect( ui->leftPane, &ItemList::addItem, this, &ContainerPage::action );
	connect( ui->rightPane, &ItemList::addItem, this, &ContainerPage::action );
}

void ContainerPage::initContainer( const QString &file, const QString &suffix )
{
	const QFileInfo f( file );
	if( !f.isFile() ) return;

	ui->containerFile->setText( f.dir().path() + QDir::separator() + f.completeBaseName() + suffix );
}

void ContainerPage::transition( ContainerState state, const QStringList &files )
{
	ui->leftPane->stateChange( state );
	ui->rightPane->stateChange( state );

	switch( state )
	{
	case UnsignedContainer:
		ui->leftPane->clear();
		ui->rightPane->clear();
		if( !files.isEmpty() ) initContainer( files[0], ".bdoc" );
		hideRightPane();
		ui->leftPane->init( ItemList::File, "Allkirjastamiseks valitud failid" );
		showMainAction( SignatureAdd, "ALLKIRJASTA\nID-KAARDIGA" );
		showButtons( { ui->cancel, ui->save } );
		hideButtons( { ui->encrypt, ui->navigateToContainer, ui->email } );
		break;
	case UnsignedSavedContainer:
		break;
	case SignedContainer:
		ui->leftPane->init( ItemList::File, "Kontaineri failid" );
		showRightPane( ItemList::Signature, "Kontaineri allkirjad" );
		ui->rightPane->clear();
		ui->rightPane->add( SignatureAdd );
		hideMainAction();
		hideButtons( { ui->cancel, ui->save } );
		showButtons( { ui->encrypt, ui->navigateToContainer, ui->email } );
		break;
	case UnencryptedContainer:
		ui->leftPane->clear();
		ui->rightPane->clear();
		if( !files.isEmpty() ) initContainer( files[0], ".cdoc" );
		ui->leftPane->init( ItemList::File, "Krüpteerimiseks valitud failid" );
		showRightPane( ItemList::Address, "Adressaadid" );
		showMainAction( EncryptContainer, "KRÜPTEERI" );
		showButtons( { ui->cancel } );
		hideButtons( { ui->encrypt, ui->save, ui->navigateToContainer, ui->email } );
		break;
	case EncryptedContainer:
		ui->leftPane->init( ItemList::File, "Krüpteeritud failid" );
		ui->rightPane->clear();
		showRightPane( ItemList::Address, "Adressaadid" );
		hideMainAction();
		hideButtons( { ui->encrypt, ui->cancel, ui->save } );
		showButtons( { ui->navigateToContainer, ui->email } );
		break;
	case DecryptedContainer:
		break;
	}

	for( auto file: files )
	{
		ui->leftPane->addFile( file );
	}
}

void ContainerPage::hideButtons( std::vector<QWidget*> buttons )
{
	for( auto *button: buttons ) button->hide();
}

void ContainerPage::hideMainAction()
{
	if( mainAction )
	{
		mainAction->hide();
	}
	ui->mainActionSpacer->changeSize( 1, 20, QSizePolicy::Fixed );
}

void ContainerPage::hideOtherAction()
{
	if(!otherAction)
		return;

	otherAction->close();
	otherAction.reset();
	mainAction->setStyleSheet( "QPushButton { border-top-left-radius: 2px; }" );
}

void ContainerPage::hideRightPane()
{
	ui->rightPane->hide();
}

void ContainerPage::mobileDialog()
{
	hideOtherAction();
	MobileDialog dlg( qApp->activeWindow() );
	if( dlg.exec() == QDialog::Accepted )
	{
		emit action( SignatureMobile );
	}
}

void ContainerPage::setContainer( const QString &file )
{
	const QFileInfo f( file );
	// TODO check if file
	// if( !f.isFile() ) return Other;

	ui->containerFile->setText( file );
}

void ContainerPage::showButtons( std::vector<QWidget*> buttons )
{
	for( auto *button: buttons ) button->show();
}

void ContainerPage::showDropdown()
{
	if( otherAction )
	{
		hideOtherAction();
	}
	else
	{
		otherAction.reset( new MainAction( SignatureMobile, "ALLKIRJASTA\nMOBIILI-ID’GA", this, false ) );
		otherAction->move( this->width() - ACTION_WIDTH, this->height() - ACTION_HEIGHT * 2 - 1 );
		connect( otherAction.get(), &MainAction::action, this, &ContainerPage::mobileDialog );

		otherAction->show();
		mainAction->setStyleSheet( "QPushButton { border-top-left-radius: 0px; }" );
		otherAction->setStyleSheet( "QPushButton { border-top-left-radius: 2px; border-top-right-radius: 2px; }" );
	}

}

void ContainerPage::showRightPane( ItemList::ItemType itemType, const QString &header )
{
	ui->rightPane->init( itemType, header );
	ui->rightPane->show();
}

void ContainerPage::showMainAction( Actions action, const QString &label )
{
	if( mainAction )
	{
		mainAction->update( action, label, action == SignatureAdd );
		mainAction->show();
	}
	else
	{
		mainAction.reset( new MainAction( action, label, this, action == SignatureAdd ) );
		mainAction->move( this->width() - ACTION_WIDTH, this->height() - ACTION_HEIGHT );
		connect( mainAction.get(), &MainAction::action, this, &ContainerPage::action );
		connect( mainAction.get(), &MainAction::action, this, &ContainerPage::hideOtherAction );
		connect( mainAction.get(), &MainAction::dropdown, this, &ContainerPage::showDropdown );
		mainAction->show();
	}
	ui->mainActionSpacer->changeSize( 198, 20, QSizePolicy::Fixed );	
}

void ContainerPage::resizeEvent( QResizeEvent *event )
{
	if( mainAction )
	{
		mainAction->move( this->width() - ACTION_WIDTH, this->height() - ACTION_HEIGHT );
	}
	if( otherAction )
	{
		otherAction->move( this->width() - ACTION_WIDTH, this->height() - ACTION_HEIGHT * 2 - 1 );
	}
}
