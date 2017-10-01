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
#include "Application.h"
#include "QPCSC.h"
#include "QSigner.h"
#include "Styles.h"
#include "XmlReader.h"
#include "effects/FadeInNotification.h"
#include "effects/ButtonHoverFilter.h"
#include "dialogs/AddRecipients.h"
#include "dialogs/SettingsDialog.h"
#include "dialogs/FirstRun.h"
#include "util/FileUtil.h"

#include <common/Settings.h>
#include <common/SslCertificate.h>
#include <common/DateTime.h>

#include <QDebug>
#include <QFileDialog>

using namespace ria::qdigidoc4;

MainWindow::MainWindow( QWidget *parent ) :
	QWidget( parent ),
	ui( new Ui::MainWindow )
{
	QFont condensed11 = Styles::font( Styles::Condensed, 11 );
	QFont condensed14 = Styles::font( Styles::Condensed, 14 );
	QFont regular14 = Styles::font( Styles::Regular, 14 );
	QFont regular20 = Styles::font( Styles::Regular, 20 );

	ui->setupUi(this);
	
	ui->version->setFont( Styles::font( Styles::Regular, 12 ) );
	ui->version->setText( "Ver. " + qApp->applicationVersion() );

	coatOfArms.reset( new QSvgWidget( ui->logo ) );
	coatOfArms->load( QString( ":/images/Logo_small.svg" ) );
	coatOfArms->resize( 80, 32 );
	coatOfArms->move( 15, 17 );
	ui->signature->init( Pages::SignIntro, ui->signatureShadow, true );
	ui->crypto->init( Pages::CryptoIntro, ui->cryptoShadow, false );
	ui->myEid->init( Pages::MyEid, ui->myEidShadow, false );

	connect( ui->signature, &PageIcon::activated, this, &MainWindow::pageSelected );
	connect( ui->crypto, &PageIcon::activated, this, &MainWindow::pageSelected );
	connect( ui->myEid, &PageIcon::activated, this, &MainWindow::pageSelected );

	selector.reset( new DropdownButton( ":/images/arrow_down.svg", ":/images/arrow_down_selected.svg", ui->idSelector ) );
	selector->hide();
	selector->resize( 12, 6 );
	selector->move( 339, 32 );
	selector->setCursor( Qt::PointingHandCursor );
	ui->help->installEventFilter( new ButtonHoverFilter( ":/images/icon_Abi.svg", ":/images/icon_Abi_hover.svg", this ) );
	ui->settings->installEventFilter( new ButtonHoverFilter( ":/images/icon_Seaded.svg", ":/images/icon_Seaded_hover.svg", this ) );
	buttonGroup = new QButtonGroup( this );
	buttonGroup->addButton( ui->help, HeadHelp );
	buttonGroup->addButton( ui->settings, HeadSettings );

	ui->warningText->setFont( regular14 );
	ui->warningAction->setFont( Styles::font( Styles::Regular, 14, QFont::Bold ) );

	ui->signIntroLabel->setFont( regular20 );
	ui->signIntroButton->setFont( condensed14 );
	ui->cryptoIntroLabel->setFont( regular20 );
	ui->cryptoIntroButton->setFont( condensed14 );
	
	ui->help->setFont( condensed11 );
	ui->settings->setFont( condensed11 );

#ifdef Q_OS_WIN
	// Add grey line on Windows in order to separate white title bar from card selection bar
	QWidget *separator = new QWidget(this);
	separator->setFixedHeight(1);
	separator->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
	separator->setStyleSheet(QString("background-color: #D9D9D9;"));
	separator->resize(8000, 1);
	separator->move(110, 0);
	separator->show();
#endif
	QGraphicsDropShadowEffect *bodyShadow = new QGraphicsDropShadowEffect;
	bodyShadow->setBlurRadius(9.0);
	bodyShadow->setColor(QColor(0, 0, 0, 160));
	bodyShadow->setOffset(4.0);
	ui->topBar->setGraphicsEffect(bodyShadow);

	setAcceptDrops( true );
	smartcard = new QSmartCard( this );
	// Refresh ID card info in card widget
	connect( smartcard, &QSmartCard::dataChanged, this, &MainWindow::showCardStatus );
	// Refresh card info in card pop-up menu
	connect( smartcard, &QSmartCard::dataLoaded, this, &MainWindow::updateCardData );
	// Show card pop-up menu
	connect( selector.get(), &DropdownButton::dropdown, this, &MainWindow::showCardMenu );
	smartcard->start();

	ui->accordion->init();

	hideWarningArea();

	connect( ui->signIntroButton, &QPushButton::clicked, [this]() { navigateToPage(SignDetails); } );
	connect( ui->cryptoIntroButton, &QPushButton::clicked, [this]() { navigateToPage(CryptoDetails); } );
	connect( buttonGroup, static_cast<void(QButtonGroup::*)(int)>(&QButtonGroup::buttonClicked), this, &MainWindow::buttonClicked );
	connect( ui->signContainerPage, &ContainerPage::action, this, &MainWindow::onSignAction );
	connect( ui->cryptoContainerPage, &ContainerPage::action, this, &MainWindow::onCryptoAction );
	connect( ui->accordion, &Accordion::checkEMail, this, &MainWindow::getEmailStatus );   // To check e-mail
	connect( ui->accordion, &Accordion::activateEMail, this, &MainWindow::activateEmail );   // To activate e-mail
	connect( ui->infoStack, &InfoStack::photoClicked, this, &MainWindow::photoClicked );
	connect( ui->cardInfo, &CardWidget::photoClicked, this, &MainWindow::photoClicked );   // To load photo
	connect( ui->cardInfo, &CardWidget::selected, this, [this]() { if( selector ) selector->press(); } );

	showCardStatus();
	connect( ui->accordion, SIGNAL( changePin1Clicked( bool, bool ) ), this, SLOT( changePin1Clicked( bool, bool ) ) );
	connect( ui->accordion, SIGNAL( changePin2Clicked( bool, bool ) ), this, SLOT( changePin2Clicked( bool, bool ) ) );
	connect( ui->accordion, SIGNAL( changePukClicked( bool ) ), this, SLOT( changePukClicked( bool ) ) );
	connect( ui->accordion, SIGNAL( certDetailsClicked( QString ) ), this, SLOT( certDetailsClicked( QString ) ) );
}

MainWindow::~MainWindow()
{
	delete ui;
	delete buttonGroup;
	delete smartcard;
}

void MainWindow::pageSelected( PageIcon *const page )
{
	selectPageIcon( page );
	navigateToPage( page->getType() );
}

void MainWindow::buttonClicked( int button )
{
	switch( button )
	{
	case HeadHelp:
	{
		//QDesktopServices::openUrl( QUrl( Common::helpUrl() ) );
		//showWarning( "Not implemented yet" );
		FirstRun dlg(this);
		dlg.exec();
		break;
	}
	case HeadSettings:
	{
		// qApp->showSettings();
		// showNotification( "Not implemented yet" );
		SettingsDialog dlg(this);
		dlg.exec();
		break;
	}
	default:
		break;
	}
}

void MainWindow::cachePicture( const QString &id, const QImage &image )
{
	Settings settings;
	QVariantList index = settings.value("imageIndex").toList();
	QVariantMap images = settings.value("imageCache").toMap();
	index.insert( 0, id );
	images[id] = image;
	if( index.size() > 10 )
	{
		QString removedId = index.takeLast().toString();
		images.remove( removedId );
	}
	settings.setValue( "imageIndex", index ); 
	settings.setValue( "imageCache", images );
}

void MainWindow::clearOverlay()
{
	overlay->close();
	overlay.reset( nullptr );
}

void MainWindow::showOverlay( QWidget *parent )
{
	overlay.reset( new Overlay(parent) );
	overlay->show();
}

void MainWindow::dragEnterEvent(QDragEnterEvent *event)
{
	event->acceptProposedAction();

	showOverlay(this);
}

void MainWindow::dragLeaveEvent(QDragLeaveEvent *event)
{
	event->accept();

	clearOverlay();
}

void MainWindow::dropEvent(QDropEvent *event)
{
	const QMimeData *mimeData = event->mimeData();
	QStringList files;
	qDebug() << "Dropped!";

	if (mimeData->hasUrls()) 
	{
		qDebug() << "Dropped urls";
		for( auto url: mimeData->urls())
		{
			if (url.scheme() == "file" )
			{
				qDebug() << url.toLocalFile();
				files << url.toLocalFile();
			}
			
		}
	} 
	else
	{
		showNotification( "Unrecognized data" );
	}
	event->acceptProposedAction();
	clearOverlay();

	if( !files.isEmpty() )
	{
		openFiles( files );
	}
}

void MainWindow::hideCardPopup()
{
	selector->init();
	showCardMenu( false );
}

void MainWindow::hideWarningArea()
{
	ui->topBarShadow->setStyleSheet("background: qlineargradient(x1: 0, y1: 0, x2: 0, y2: 1, stop: 0 #c8c8c8, stop: 1 #F4F5F6); \nborder: none;");
	ui->warning->hide();
}

// Mouse click on warning region will hide it.
void MainWindow::mousePressEvent(QMouseEvent *event)
{
	if(ui->warning->underMouse())
		hideWarningArea();
}

void MainWindow::navigateToPage( Pages page, const QStringList &files, bool create )
{
	ui->startScreen->setCurrentIndex(page);
	
	if( page == SignDetails)
	{
		if( create )
		{
			ui->signContainerPage->transition( ContainerState::UnsignedContainer, files );
		}
		else
		{
			ui->signContainerPage->transition( ContainerState::SignedContainer );
			if( !files.isEmpty() ) ui->signContainerPage->setContainer( files[0] );
		}
	}
	else if( page == CryptoDetails)
	{
		if( create )
		{
			ui->cryptoContainerPage->transition( ContainerState::UnencryptedContainer, files );
		}
		else
		{
			ui->cryptoContainerPage->transition( ContainerState::EncryptedContainer );
			if( !files.isEmpty() ) ui->cryptoContainerPage->setContainer( files[0] );
		}
	}
}

void MainWindow::onSignAction( int action )
{
	if( action == SignatureAdd || action == SignatureMobile )
	{
		ui->signContainerPage->transition(ContainerState::SignedContainer);

		FadeInNotification* notification = new FadeInNotification( this, "#ffffff", "#8CC368", 110 );
		notification->start( "Konteiner on edukalt allkirjastatud!", 750, 1500, 600 );
	}
	else if( action == ContainerCancel )
	{
		navigateToPage(Pages::SignIntro);
	}
	else if( action == FileRemove )
	{

	}
}

void MainWindow::onCryptoAction( int action )
{
	if( action == EncryptContainer )
	{
		ui->cryptoContainerPage->transition(ContainerState::EncryptedContainer);

		FadeInNotification* notification = new FadeInNotification( this, "#ffffff", "#53c964", 110 );
		notification->start( "Krüpteerimine õnnestus!", 750, 1500, 600 );
	}
	else if( action == ContainerCancel )
	{
		navigateToPage(Pages::CryptoIntro);
	}
	else if( action == FileRemove )
	{

	}
	else if( action == AddressAdd )
	{
		AddRecipients dlg(this);
		dlg.exec();
	}
}

void MainWindow::openFiles( const QStringList files )
{
/*
	1. If containers are not open:
	1.1 If one file and known filetype
			- Open either
				-- signed (sign. container)
				-- or encrypted (crypto container)
	1.2 else (Unknown type/multiple files):
			- If on encrypt page, open encryption view
			- else open signing view

	2. If container open:
	2.1 if UnsignedContainer | UnsignedSavedContainer
		- Add file to files to be signed
	2.2 else If UnencryptedContainer
		- Add file to files to be encrypted
	2.3 else if SignedContainer
		- ask if should be added or handle open
	2.4 else if EncryptedContainer || DecryptedContainer
			- ask if should be closed and handle open 
*/
	auto current = ui->startScreen->currentIndex();
	Pages page = SignDetails;
	bool create = true;
	if( current != SignDetails && current != CryptoDetails )
	{
		if( files.size() == 1 )
		{
			auto fileType = FileUtil::detect( files[0] );
			if( fileType == CryptoContainer )
			{
				page = CryptoDetails;
				create = false;
			}
			else if( fileType == SignatureContainer )
			{
				page = SignDetails;
				create = false;
			}
			else if( current == CryptoIntro )
			{
				page = CryptoDetails;
			}
		}
	}
	else
	{
		// TODO (2.)
		page = current == CryptoIntro ? CryptoIntro : SignIntro;
	}

	selectPageIcon( page == CryptoDetails ? ui->crypto : ui->signature );
	navigateToPage( page, files, create );
}

void MainWindow::resizeEvent( QResizeEvent *event )
{
	ui->version->move( ui->version->geometry().x(), ui->leftBar->height() - ui->version->height() - 11 );
}

void MainWindow::selectPageIcon( PageIcon* page )
{
	ui->rightShadow->raise();
	for( auto pageIcon: { ui->signature, ui->crypto, ui->myEid } )
	{
		pageIcon->activate( pageIcon == page );
	}
}

void MainWindow::showCardMenu( bool show )
{
	if( show )
	{
		cardPopup.reset( new CardPopup( smartcard, this ) );
		// To select active card from several cards in readers ..
		connect( cardPopup.get(), &CardPopup::activated, smartcard, &QSmartCard::selectCard, Qt::QueuedConnection );
		// .. and hide card popup menu
		connect( cardPopup.get(), &CardPopup::activated, this, &MainWindow::hideCardPopup );
		cardPopup->show();
	}
	else if( cardPopup )
	{
		cardPopup->close();
	}
}

void MainWindow::showCardStatus()
{
	Application::restoreOverrideCursor();
	QSmartCardData t = smartcard->data();

	if( !t.isNull() )
	{
		ui->idSelector->show();
		ui->infoStack->show();
		ui->accordion->show();
		ui->noCardInfo->hide();

		ui->cardInfo->update( QSharedPointer<const QCardInfo>(new QCardInfo( t )) );

		if( t.authCert().type() & SslCertificate::EstEidType )
		{
			Styles::cachedPicture( t.data(QSmartCardData::Id ).toString(), { ui->cardInfo, ui->infoStack } );
		}
		ui->infoStack->update( t );
		ui->accordion->updateInfo( smartcard );
		ui->myEid->invalidCertIcon( !t.authCert().isValid() || !t.signCert().isValid() );
		ui->myEid->pinIsBlockedIcon( 
				t.retryCount( QSmartCardData::Pin1Type ) == 0 || 
				t.retryCount( QSmartCardData::Pin2Type ) == 0 || 
				t.retryCount( QSmartCardData::PukType ) == 0 );
	}
	else
	{
		if ( !QPCSC::instance().serviceRunning() )
			noReader_NoCard_Loading_Event( "PCSC service is not running" );
		else
		if ( t.readers().isEmpty() )
			noReader_NoCard_Loading_Event( "No readers found" );
		else if ( t.card().isEmpty() )
			noReader_NoCard_Loading_Event( "Lugejas ei ole kaarti. Kontrolli, kas ID-kaart on õiget pidi lugejas." );
		else
			noReader_NoCard_Loading_Event( "Loading data", true );
	}

	// Combo box to select the cards from
	if( t.cards().size() > 1 )
	{
		selector->show();
	}
	else
	{
		selector->hide();
		hideCardPopup();
	}
}

void MainWindow::updateCardData()
{
	if( cardPopup )
		cardPopup->update( smartcard );
}

void MainWindow::noReader_NoCard_Loading_Event( const QString &text, bool isLoading )
{
	ui->idSelector->hide(); 
	if( isLoading )
		Application::setOverrideCursor( Qt::BusyCursor );

	ui->noCardInfo->show();
	ui->noCardInfo->update( text );
	ui->noCardInfo->setAccessibleDescription( text );
	ui->infoStack->clearData();
	ui->cardInfo->clearPicture();
	ui->infoStack->clearPicture();
	ui->infoStack->hide();
	ui->accordion->hide();
	ui->accordion->updateOtherData( false );
	ui->myEid->invalidCertIcon( false );
	ui->myEid->pinIsBlockedIcon( false );
}

// Loads picture 
void MainWindow::photoClicked( const QPixmap *photo )
{
	if( photo )
		return savePhoto( photo );

	QByteArray buffer = sendRequest( SSLConnect::PictureInfo );

	if( buffer.isEmpty() )
		return;

	QImage image;
	if( !image.loadFromData( buffer ) )
	{
		XmlReader xml( buffer );
		QString error;
		xml.readEmailStatus( error );
		if( !error.isEmpty() )
			showWarning( XmlReader::emailErr( error.toUInt() ), QString() );
		return;
	}

	cachePicture( smartcard->data().data(QSmartCardData::Id).toString(), image );
	QPixmap pixmap = QPixmap::fromImage( image );
	ui->cardInfo->showPicture( pixmap );
	ui->infoStack->showPicture( pixmap );
}

void MainWindow::savePhoto( const QPixmap *photo )
{
	QString fileName = QFileDialog::getSaveFileName(this,
		"Salvesta foto", "",
		"Foto (*.jpg);;All Files (*)");

	photo->save( fileName, "JPEG", 100 );
}

void MainWindow::showWarning( const QString &msg, const QString &details )
{
	ui->topBarShadow->setStyleSheet("background: qlineargradient(x1: 0, y1: 0, x2: 0, y2: 1, stop: 0 #b5aa92, stop: 1 #F8DDA7); \nborder: none;");	
	ui->warning->show();
	ui->warningText->setText( msg );
	ui->warningAction->setText( details );
	ui->warningAction->setTextFormat( Qt::RichText );
	ui->warningAction->setTextInteractionFlags( Qt::TextBrowserInteraction );
	ui->warningAction->setOpenExternalLinks( true );	
}
