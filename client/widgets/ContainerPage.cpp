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

#include "DigiDoc.h"
#include "Styles.h"
#include "dialogs/MobileDialog.h"
#include "widgets/AddressItem.h"
#include "widgets/FileItem.h"
#include "widgets/SignatureItem.h"

#include <QDebug>
#include <QFileDialog>
#include <QDir>
#include <QFileInfo>
#include <QFontMetrics>
#include <QGraphicsDropShadowEffect>
#include <QMessageBox>
#include <QProgressBar>
#include <QProgressDialog>

using namespace ria::qdigidoc4;


#define ACTION_WIDTH 200
#define ACTION_HEIGHT 65


ContainerPage::ContainerPage(QWidget *parent)
: QWidget(parent)
, ui(new Ui::ContainerPage)
, containerFont(Styles::font(Styles::Regular, 14))
, fm(QFontMetrics(containerFont))
{
	ui->setupUi( this );
	init();
}

ContainerPage::~ContainerPage()
{
	delete ui;
}

void ContainerPage::clear()
{
	ui->leftPane->clear();
	ui->rightPane->clear();
}

void ContainerPage::cardSigning(bool enable)
{
	if(cardInReader != enable)
	{
		cardInReader = enable;
		if(mainAction)
		{
			showSigningButton();
		}
	}
}

void ContainerPage::elideFileName(bool force)
{
	if(ui->containerFile->width() < containerFileWidth)
	{
		elided = true;
		ui->containerFile->setText(fm.elidedText(fileName, Qt::ElideMiddle, ui->containerFile->width()));
	}
	else if(elided || force)
	{
		elided = false;
		ui->containerFile->setText(fileName);
	}
}

void ContainerPage::init()
{
	ui->leftPane->init(fileName);

	ui->container->setFont(containerFont);
	ui->containerFile->setFont(containerFont);

	ui->changeLocation->setIcons( "/images/icon_Edit.svg", "/images/icon_Edit_hover.svg", "/images/icon_Edit_pressed.svg", 4, 4, 18, 18 );
	ui->changeLocation->init( LabelButton::BoxedDeepCeruleanWithCuriousBlue, "MUUDA", Actions::ContainerLocation );
	ui->cancel->init( LabelButton::BoxedMojo, "← KATKESTA", Actions::ContainerCancel );
	ui->encrypt->init( LabelButton::BoxedDeepCerulean, "KRÜPTEERI", Actions::ContainerEncrypt );
	ui->navigateToContainer->init( LabelButton::BoxedDeepCerulean, "AVA KONTAINERI ASUKOHT", Actions::ContainerNavigate );
	ui->email->init( LabelButton::BoxedDeepCerulean, "EDASTA E-MAILIGA", Actions::ContainerEmail );
	ui->save->init( LabelButton::BoxedDeepCerulean, "SALVESTA ALLKIRJASTAMATA", Actions::ContainerSave );

	connect(ui->cancel, &LabelButton::clicked, this, &ContainerPage::forward);
	connect(ui->save, &LabelButton::clicked, this, &ContainerPage::forward);
	connect(ui->leftPane, &ItemList::addItem, this, &ContainerPage::forward);
	connect(ui->rightPane, &ItemList::addItem, this, &ContainerPage::forward);
}

void ContainerPage::forward(int code)
{
	emit action(code);
}

void ContainerPage::initContainer( const QString &file, const QString &suffix )
{
	const QFileInfo f( file );
	if( !f.isFile() ) return;

	fileName = f.dir().path() + QDir::separator() + f.completeBaseName() + suffix;
	ui->containerFile->setText(fileName);
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
		emit action(SignatureMobile, dlg.idCode(), dlg.phoneNo());
	}
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

	if(event->oldSize().width() != event->size().width())
		elideFileName();
}

void ContainerPage::setHeader(const QString &file)
{
	fileName = file;
	containerFileWidth = fm.width(fileName);
	elideFileName(true);
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
		mainAction->show();
	}

	for(auto conn: actionConnections)
		QObject::disconnect(conn);
	actionConnections.clear();
	if(action == SignatureMobile)
	{
		actionConnections.push_back(connect(mainAction.get(), &MainAction::action, this, &ContainerPage::mobileDialog));
	}
	else
	{
		actionConnections.push_back(connect(mainAction.get(), &MainAction::action, this, &ContainerPage::forward));
		actionConnections.push_back(connect(mainAction.get(), &MainAction::action, this, &ContainerPage::hideOtherAction));
		actionConnections.push_back(connect(mainAction.get(), &MainAction::dropdown, this, &ContainerPage::showDropdown));	
	}

	ui->mainActionSpacer->changeSize( 198, 20, QSizePolicy::Fixed );
}

void ContainerPage::showSigningButton()
{
	hideOtherAction();
	if(cardInReader)
	{
		showMainAction(SignatureAdd, "ALLKIRJASTA\nID-KAARDIGA");
	}
	else
	{
		showMainAction(SignatureMobile, "ALLKIRJASTA\nMOBIILI-ID’GA");
	}
}

void ContainerPage::transition(CryptoDoc* container)
{
	clear();
	ContainerState state = container->state();
	ui->leftPane->stateChange(state);
	ui->rightPane->stateChange(state);

	setHeader(container->fileName());

	for(const CKey &key: container->keys())
		ui->rightPane->addWidget(new AddressItem(key, state, ui->rightPane));

	ui->leftPane->setModel(container->documentModel());
	updatePanes(state);
}

void ContainerPage::transition(DigiDoc* container)
{
	clear();
	ContainerState state = container->state();
	ui->leftPane->stateChange(state);
	ui->rightPane->stateChange(state);

	setHeader(container->fileName());
	DigiDocSignature::SignatureStatus status = DigiDocSignature::Valid;

	if(!container->timestamps().isEmpty())
	{
		ui->rightPane->addHeader("Konteineri ajatemplid");

		for(const DigiDocSignature &c: container->timestamps())
			ui->rightPane->addHeaderWidget(new SignatureItem(c, state, ui->rightPane));
	}

	for(const DigiDocSignature &c: container->signatures())
		ui->rightPane->addWidget(new SignatureItem(c, state, ui->rightPane));

	ui->leftPane->setModel(container->documentModel());
	updatePanes(state);

	// TODO Show invalid signature status
	// switch( status )
	// {
	// case DigiDocSignature::Invalid: viewSignaturesError->setText( tr("NB! Invalid signature") ); break;
	// case DigiDocSignature::Unknown: viewSignaturesError->setText( "<i>" + tr("NB! Status unknown") + "</i>" ); break;
	// case DigiDocSignature::Test: viewSignaturesError->setText( tr("NB! Test signature") ); break;
	// case DigiDocSignature::Warning: viewSignaturesError->setText( "<font color=\"#FFB366\">" + tr("NB! Signature contains warnings") + "</font>" ); break;
	// case DigiDocSignature::NonQSCD: viewSignaturesError->clear(); break;
	// case DigiDocSignature::Valid: viewSignaturesError->clear(); break;
	// }
	// break;
}

void ContainerPage::updatePanes(ContainerState state)
{
	auto buttonWidth = ui->changeLocation->width();
	bool resize = false;

	switch( state )
	{
	case UnsignedContainer:
		ui->changeLocation->show();
		ui->rightPane->clear();
		hideRightPane();
		ui->leftPane->init(fileName);
		showSigningButton();
		ui->cancel->setText("← KATKESTA");
		showButtons( { ui->cancel, ui->save } );
		hideButtons( { ui->encrypt, ui->navigateToContainer, ui->email } );
		break;
	case UnsignedSavedContainer:
		resize = !ui->changeLocation->isHidden();
		ui->leftPane->init(fileName);
		showSigningButton();
		ui->cancel->setText("← ALGUSESSE");
		showButtons( { ui->cancel, ui->encrypt, ui->navigateToContainer, ui->email } );
		hideButtons( { ui->save } );
		showRightPane( ItemList::Signature, "Kontaineri allkirjad puuduvad" );
		break;
	case SignedContainer:
		resize = !ui->changeLocation->isHidden();
		ui->changeLocation->hide();
		ui->leftPane->init(fileName);
		showRightPane( ItemList::Signature, "Kontaineri allkirjad" );
		hideMainAction();
		hideButtons( { ui->cancel, ui->save } );
		showButtons( { ui->encrypt, ui->navigateToContainer, ui->email } );
		break;
	case UnencryptedContainer:
		ui->changeLocation->show();
		ui->leftPane->init(fileName);
		showRightPane( ItemList::Address, "Adressaadid" );
		showMainAction( EncryptContainer, "KRÜPTEERI" );
		showButtons( { ui->cancel } );
		hideButtons( { ui->encrypt, ui->save, ui->navigateToContainer, ui->email } );
		break;
	case EncryptedContainer:
		resize = !ui->changeLocation->isHidden();
		ui->changeLocation->hide();
		ui->leftPane->init(fileName, "Krüpteeritud failid");
		showRightPane( ItemList::Address, "Adressaadid" );
		showMainAction( DecryptContainer, "DEKRÜPTEERI\nID-KAARDIGA" );
		hideButtons( { ui->encrypt, ui->cancel, ui->save } );
		showButtons( { ui->navigateToContainer, ui->email } );
		break;
	case DecryptedContainer:
		resize = !ui->changeLocation->isHidden();
		ui->changeLocation->hide();
		ui->leftPane->init(fileName, "Dekrüpteeritud failid");
		showRightPane( ItemList::Address, "Adressaadid" );
		hideMainAction();
		hideButtons( { ui->encrypt, ui->cancel, ui->save } );
		showButtons( { ui->navigateToContainer, ui->email } );
		break;
	default:
		// Uninitialized cannot be shown on container page
		break;
	}

	if(resize)
	{
		// Forcibly resize the filename widget after hiding button
		ui->containerFile->resize(ui->containerFile->width() + buttonWidth,
		ui->containerFile->height());
	}
}
