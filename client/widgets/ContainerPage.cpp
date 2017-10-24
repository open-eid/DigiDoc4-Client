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
#include "Settings.h"
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

void ContainerPage::changeCard(const QString& idCode)
{
	if(cardInReader != idCode)
	{
		cardInReader = idCode;
		if(mainAction && (ui->leftPane->getState() & SignatureContainers))
			showSigningButton();
	}
}

void ContainerPage::elideFileName(bool force)
{
	if(ui->containerFile->width() < containerFileWidth)
	{
		elided = true;
		ui->containerFile->setText("<a href='#browse-Container'><span style='color:rgb(53, 55, 57)'>" +
			fm.elidedText(fileName, Qt::ElideMiddle, ui->containerFile->width()) + "</span></a>");
	}
	else if(elided || force)
	{
		elided = false;
		ui->containerFile->setText("<a href='#browse-Container'><span style='color:rgb(53, 55, 57)'>" + fileName + "</span></a>");
	}
}

void ContainerPage::init()
{
	ui->leftPane->init(fileName);

	ui->container->setFont(containerFont);
	ui->containerFile->setFont(containerFont);

	ui->changeLocation->setIcons( "/images/icon_Edit.svg", "/images/icon_Edit_hover.svg", "/images/icon_Edit_pressed.svg", 4, 4, 18, 18 );
    ui->changeLocation->init( LabelButton::BoxedDeepCeruleanWithCuriousBlue, tr("CHANGE"), Actions::ContainerLocation );
    ui->cancel->init( LabelButton::BoxedMojo, tr("CANCEL"), Actions::ContainerCancel );
    ui->encrypt->init( LabelButton::BoxedDeepCerulean, tr("ENCRYPT"), Actions::ContainerEncrypt );
    ui->navigateToContainer->init( LabelButton::BoxedDeepCerulean, tr("OPEN CONTAINER LOCATION"), Actions::ContainerNavigate );
    ui->email->init( LabelButton::BoxedDeepCerulean, tr("SEND WITH E-MAIL"), Actions::ContainerEmail );
    ui->save->init( LabelButton::BoxedDeepCerulean, tr("SAVE UNSIGNED"), Actions::ContainerSave );

	mobileCode = Settings().value("Client/MobileCode").toString();

	connect(this, &ContainerPage::cardChanged, this, &ContainerPage::changeCard);
	connect(this, &ContainerPage::cardChanged, [this](const QString& idCode){ emit ui->rightPane->idChanged(idCode, mobileCode); });
	connect(this, &ContainerPage::moved, ui->containerFile, &QLabel::setText);
	connect(ui->changeLocation, &LabelButton::clicked, this, &ContainerPage::forward);
	connect(ui->cancel, &LabelButton::clicked, this, &ContainerPage::forward);
	connect(ui->save, &LabelButton::clicked, this, &ContainerPage::forward);
	connect(ui->leftPane, &FileList::addFiles, this, &ContainerPage::addFiles);
	connect(ui->leftPane, &ItemList::addItem, this, &ContainerPage::forward);
	connect(ui->leftPane, &ItemList::removed, this, &ContainerPage::removed);
	connect(ui->rightPane, &ItemList::addItem, this, &ContainerPage::forward);
	connect(ui->rightPane, &ItemList::removed, this, &ContainerPage::removed);
	connect(ui->email, &LabelButton::clicked, this, &ContainerPage::forward);
	connect(ui->navigateToContainer, &LabelButton::clicked, this, &ContainerPage::forward);
	connect(ui->convert, &LabelButton::clicked, this, &ContainerPage::forward);
	connect(ui->containerFile, &QLabel::linkActivated, this, &ContainerPage::browseContainer );
}

void ContainerPage::forward(int code)
{
	emit action(code);
}

void ContainerPage::browseContainer(QString link)
{
	emit action(Actions::ContainerNavigate, link);
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
	MobileDialog dlg(qApp->activeWindow());
	QString newCode = Settings().value("Client/MobileCode").toString();
	if( dlg.exec() == QDialog::Accepted )
	{
		emit action(SignatureMobile, dlg.idCode(), dlg.phoneNo());
	}

	if (newCode != mobileCode)
	{
		mobileCode = newCode;
		emit cardChanged(cardInReader);
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
        otherAction.reset( new MainAction( SignatureMobile, tr("SIGN WITH\nMOBILE ID"), this, false ) );
		otherAction->move( this->width() - ACTION_WIDTH, this->height() - ACTION_HEIGHT * 2 - 1 );
		connect( otherAction.get(), &MainAction::action, this, &ContainerPage::mobileDialog );

		otherAction->show();
		mainAction->setStyleSheet( "QPushButton { border-top-left-radius: 0px; }" );
		otherAction->setStyleSheet( "QPushButton { border-top-left-radius: 2px; border-top-right-radius: 2px; }" );
	}

}

void ContainerPage::showRightPane(ItemType itemType, const QString &header)
{
	ui->rightPane->init(itemType, header);
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
	if(cardInReader.isNull())
        showMainAction(SignatureAdd, tr("ALLKIRJASTA\nID-KAARDIGA"));
	else
        showMainAction(SignatureMobile, tr("SIGN WITH\nMOBILE ID"));
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
        ui->rightPane->addHeader(tr("Container's time stamps"));

		for(const DigiDocSignature &c: container->timestamps())
			ui->rightPane->addHeaderWidget(new SignatureItem(c, state, ui->rightPane));
	}

	bool enableSigning = container->isSupported();
	for(const DigiDocSignature &c: container->signatures())
	{
		SignatureItem *item = new SignatureItem(c, state, ui->rightPane);
		ui->rightPane->addWidget(item);
		if(enableSigning && item->isSelfSigned(cardInReader, mobileCode))
			enableSigning = false;
	}

	if(enableSigning)
		showSigningButton();
	else
		hideMainAction();

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
		ui->cancel->setText(tr("CANCEL"));
		showButtons( { ui->cancel, ui->convert, ui->save } );
		hideButtons( { ui->navigateToContainer, ui->email } );
		break;
	case UnsignedSavedContainer:
		ui->changeLocation->show();
		ui->leftPane->init(fileName);
		ui->cancel->setText(tr("STARTING"));
		showButtons( { ui->cancel, ui->convert, ui->navigateToContainer, ui->email } );
		hideButtons( { ui->save } );
		showRightPane( ItemSignature, tr("Container's signatures are missing"));
		break;
	case SignedContainer:
		resize = !ui->changeLocation->isHidden();
		ui->changeLocation->hide();
		ui->leftPane->init(fileName);
		showRightPane( ItemSignature, tr("Container's signatures") );
		ui->cancel->setText(tr("STARTING"));
		hideButtons( { ui->save } );
		showButtons( { ui->cancel, ui->convert, ui->navigateToContainer, ui->email } );
		break;
	case UnencryptedContainer:
		ui->changeLocation->show();
		ui->leftPane->init(fileName);
		showRightPane( ItemAddress, tr("Recipients") );
		showMainAction( EncryptContainer, tr("ENCRYPT") );
		ui->cancel->setText(tr("STARTING"));
		ui->convert->setText("ALLKIRJASTA");
		showButtons( { ui->cancel, ui->convert } );
		hideButtons( { ui->save, ui->navigateToContainer, ui->email } );
		break;
	case EncryptedContainer:
		resize = !ui->changeLocation->isHidden();
		ui->changeLocation->hide();
		ui->leftPane->init(fileName, tr("Encrypted files"));
		showRightPane( ItemAddress, tr("Recipients") );
		showMainAction( DecryptContainer, tr("DECRYPT WITH\nID-CARD") );
		ui->cancel->setText(tr("STARTING"));
		ui->convert->setText("ALLKIRJASTA");
		hideButtons( { ui->save } );
		showButtons( { ui->cancel, ui->convert, ui->navigateToContainer, ui->email } );
		break;
	case DecryptedContainer:
		resize = !ui->changeLocation->isHidden();
		ui->changeLocation->hide();
		ui->leftPane->init(fileName, tr("Dencrypted files"));
		showRightPane( ItemAddress, tr("Recipients"));
		hideMainAction();
		ui->cancel->setText(tr("STARTING"));
		hideButtons( { ui->convert, ui->save } );
		showButtons( { ui->cancel, ui->navigateToContainer, ui->email } );
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
