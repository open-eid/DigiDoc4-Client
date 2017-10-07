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
#include "common/SslCertificate.h"
#include "dialogs/MobileDialog.h"
#include "dialogs/WaitDialog.h"
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

	connect( ui->cancel, &LabelButton::clicked, this, &ContainerPage::action );
	connect( ui->leftPane, &ItemList::addItem, this, &ContainerPage::action );
	connect( ui->rightPane, &ItemList::addItem, this, &ContainerPage::action );
}

void ContainerPage::initContainer( const QString &file, const QString &suffix )
{
	const QFileInfo f( file );
	if( !f.isFile() ) return;

	fileName = f.dir().path() + QDir::separator() + f.completeBaseName() + suffix;
	ui->containerFile->setText(fileName);
}

void ContainerPage::transition( ContainerState state, const QStringList &files )
{
	auto buttonWidth = ui->changeLocation->width();
	bool resize = false;

	ui->leftPane->stateChange( state );
	ui->rightPane->stateChange( state );

	switch( state )
	{
	case UnsignedContainer:
		ui->changeLocation->show();
		ui->leftPane->clear();
		ui->rightPane->clear();
		if( !files.isEmpty() ) initContainer( files[0], ".bdoc" );
		hideRightPane();
		ui->leftPane->init(fileName);
		showMainAction( SignatureAdd, "ALLKIRJASTA\nID-KAARDIGA" );
		showButtons( { ui->cancel, ui->save } );
		hideButtons( { ui->encrypt, ui->navigateToContainer, ui->email } );
		break;
	case UnsignedSavedContainer:
		resize = !ui->changeLocation->isHidden();
		ui->changeLocation->hide();
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
		ui->leftPane->clear();
		ui->rightPane->clear();
		if( !files.isEmpty() ) initContainer( files[0], ".cdoc" );
		ui->leftPane->init(fileName);
		showRightPane( ItemList::Address, "Adressaadid" );
		showMainAction( EncryptContainer, "KRÜPTEERI" );
		showButtons( { ui->cancel } );
		hideButtons( { ui->encrypt, ui->save, ui->navigateToContainer, ui->email } );
		break;
	case EncryptedContainer:
		resize = !ui->changeLocation->isHidden();
		ui->changeLocation->hide();
		ui->leftPane->init(fileName);
		ui->rightPane->clear();
		showRightPane( ItemList::Address, "Adressaadid" );
		showMainAction( DecryptContainer, "DEKRÜPTEERI\nID-KAARDIGA" );
		hideButtons( { ui->encrypt, ui->cancel, ui->save } );
		showButtons( { ui->navigateToContainer, ui->email } );
		break;
	case DecryptedContainer:
		ui->leftPane->init( ItemList::File, "Krüpteeritud failid" );
		showRightPane( ItemList::Address, "Adressaadid" );
		hideMainAction();
		hideButtons( { ui->encrypt, ui->cancel, ui->save } );
		showButtons( { ui->navigateToContainer, ui->email } );
		resize = !ui->changeLocation->isHidden();
		ui->changeLocation->hide();
		break;
	}

	for( auto file: files )
	{
		ui->leftPane->addFile( file );
	}

	if(resize)
	{
		// Forcibly resize the filename widget after hiding button
		ui->containerFile->resize(ui->containerFile->width() + buttonWidth,
		ui->containerFile->height());
	}
}

void ContainerPage::transition(ContainerState state, DigiDoc* container)
{
	clear();

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
	QStringList files;
	transition(state, files);

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

void ContainerPage::setContainer( ria::qdigidoc4::Pages page, const QString &file )
{
	const QFileInfo f( file );
	// TODO check if file
	// if( !f.isFile() ) return Other;


	ui->containerFile->setText( file );

	if(page == CryptoDetails)
	{
		cryptoDoc.reset(new CryptoDoc(this));
		cryptoDoc->open( file );

		for( auto crypedFile: cryptoDoc->files())
		{
			FileItem *curr = new FileItem(crypedFile, ria::qdigidoc4::ContainerState::EncryptedContainer, this );
			ui->leftPane->addWidget( curr );
		}

		for( CKey key : cryptoDoc->keys() )
		{
			AddressItem *curr = new AddressItem(ria::qdigidoc4::ContainerState::UnsignedContainer, this);
			QString name = !key.cert.subjectInfo("GN").isEmpty() && !key.cert.subjectInfo("SN").isEmpty() ?
						key.cert.subjectInfo("GN").value(0) + " " + key.cert.subjectInfo("SN").value(0) :
						key.cert.subjectInfo("CN").value(0);

			QString type;
			switch (SslCertificate(key.cert).type())
			{
			case SslCertificate::DigiIDType:
				type = "Digi-ID";
				break;
			case SslCertificate::EstEidType:
				type = "ID-kaart";
				break;
			case SslCertificate::MobileIDType:
				type = "Mobiil-ID";
				break;
			default:
				type = "UnknownType";
				break;
			}

			curr->update(
						name,
						key.cert.subjectInfo("serialNumber").value(0),
						type,
						AddressItem::Remove );
			ui->rightPane->addWidget( curr );
		}
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

void ContainerPage::Decrypt(int action)
{
	if(action == DecryptContainer)
	{
		WaitDialog waitDialog(qApp->activeWindow());
		waitDialog.open();

		cryptoDoc->decrypt();

		QString dir = QFileDialog::getExistingDirectory(this, "Vali kataloog, kuhu failid salvestatakse");
		if( dir.isEmpty() )
			return;

		CDocumentModel *m = cryptoDoc->documents();
		for( int i = 0; i < m->rowCount(); ++i )
		{
			QModelIndex index = m->index( i, CDocumentModel::Name );
			QString dest = dir + "/" + index.data(Qt::UserRole).toString();

			if( QFile::exists( dest ) )
			{
				QMessageBox::StandardButton b = QMessageBox::warning( this, windowTitle(),
					"Selle nimega fail on juba olemas. Kas kirjutan üle?",
					QMessageBox::Yes | QMessageBox::No, QMessageBox::No );
				if( b == QMessageBox::No )
				{
					dest = QFileDialog::getSaveFileName( this, "Salvesta", dest );
					if( dest.isEmpty() )
						continue;
				}
				else
					QFile::remove( dest );
			}
			m->copy( index, dest );
		}

	}
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

		connect( mainAction.get(), &MainAction::action, this, &ContainerPage::Decrypt );
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

	if(event->oldSize().width() != event->size().width())
		elideFileName();
}
