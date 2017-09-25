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
#include "QSigner.h"
#include "Styles.h"
#include "XmlReader.h"
#include "effects/FadeInNotification.h"
#include "effects/ButtonHoverFilter.h"
#include "util/FileUtil.h"

#include <common/Settings.h>
#include <common/SslCertificate.h>
#include <common/DateTime.h>

#include <QDebug>
#include <QtNetwork/QSslConfiguration>

using namespace ria::qdigidoc4;

MainWindow::MainWindow( QWidget *parent ) :
	QWidget( parent ),
	ui( new Ui::MainWindow )
{
	QFont condensed11 = Styles::font( Styles::Condensed, 11 );
	QFont regular18 = Styles::font( Styles::Regular, 18 );
	QFont regular20 = Styles::font( Styles::Regular, 20 );

	ui->setupUi(this);
	
	ui->signature->init( Pages::SignIntro, ui->signatureShadow, true );
	ui->crypto->init( Pages::CryptoIntro, ui->cryptoShadow, false );
	ui->myEid->init( Pages::MyEid, ui->myEidShadow, false );

	connect( ui->signature, &PageIcon::activated, this, &MainWindow::pageSelected );
	connect( ui->crypto, &PageIcon::activated, this, &MainWindow::pageSelected );
	connect( ui->myEid, &PageIcon::activated, this, &MainWindow::pageSelected );
	
	ui->selector->hide();
	ui->help->installEventFilter( new ButtonHoverFilter( ":/images/icon_Abi.svg", ":/images/icon_Abi_hover.svg", this ) );
	ui->settings->installEventFilter( new ButtonHoverFilter( ":/images/icon_Seaded.svg", ":/images/icon_Seaded_hover.svg", this ) );
	buttonGroup = new QButtonGroup( this );
   	buttonGroup->addButton( ui->help, HeadHelp );
   	buttonGroup->addButton( ui->settings, HeadSettings );

	ui->signIntroLabel->setFont( regular20 );
	ui->signIntroButton->setFont( regular18 );
	ui->cryptoIntroLabel->setFont( regular20 );
	ui->cryptoIntroButton->setFont( regular18 );
	
	ui->help->setFont( condensed11 );
	ui->settings->setFont( condensed11 );

	setAcceptDrops(true);
	smartcard = new QSmartCard( this );
	smartcard->start();

	showCardStatus();
	ui->accordion->init();

	connect( ui->signIntroButton, &QPushButton::clicked, [this]() { navigateToPage(SignDetails); } );
	connect( ui->cryptoIntroButton, &QPushButton::clicked, [this]() { navigateToPage(CryptoDetails); } );
	connect( buttonGroup, static_cast<void(QButtonGroup::*)(int)>(&QButtonGroup::buttonClicked), this, &MainWindow::buttonClicked );
	connect( ui->signContainerPage, &ContainerPage::action, this, &MainWindow::onSignAction );
	connect( ui->cryptoContainerPage, &ContainerPage::action, this, &MainWindow::onCryptoAction );
	connect( ui->accordion, &Accordion::checkEMail, this, &MainWindow::getEmailStatus );   // To check e-mail
	connect( ui->accordion, &Accordion::activateEMail, this, &MainWindow::activateEmail );   // To activate e-mail
	connect( ui->cardInfo, &CardInfo::thePhotoLabelClicked, this, &MainWindow::loadCardPhoto );   // To load photo
	connect( smartcard, &QSmartCard::dataChanged, this, &MainWindow::showCardStatus );	  // To refresh ID card info

	connect( ui->selector, SIGNAL(activated(QString)), smartcard, SLOT(selectCard(QString)), Qt::QueuedConnection );	// To select between several cards in readers.
//    connect(ui->accordion, &Accordion::signBoxChangePinClicked, this, &MainWindow::signBoxChangePinClicked ); //[this](){ showWarning( "Will be implemented soon" ); } );
}

void MainWindow::signBoxChangePinClicked()
{
//    showWarning( "Will implemented soon" );

//    smartcard->pinUnblock( PinDialog::Pin2Type );
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
		//QDesktopServices::openUrl( QUrl( Common::helpUrl() ) );
		showWarning( "Not implemented yet" );
		break;
	case HeadSettings:
	{
		// qApp->showSettings();
		showWarning( "Not implemented yet" );
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

void MainWindow::dragEnterEvent(QDragEnterEvent *event)
{
	qDebug() << event->mimeData();
	event->acceptProposedAction();

	showOverlay(this);
}

void MainWindow::dragLeaveEvent(QDragLeaveEvent *event)
{
	qDebug() << "Drag left";
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
		showWarning( "Unrecognized data" );
	}
	event->acceptProposedAction();
	clearOverlay();

	if( !files.isEmpty() )
	{
		openFiles( files );
	}
}

void MainWindow::loadCachedPicture( const QString &id )
{
	Settings settings;
	QVariantList index = settings.value("imageIndex").toList();

	if( index.contains(id) )
	{
		QImage image = settings.value("imageCache").toMap().value( id ).value<QImage>();
		QPixmap pixmap = QPixmap::fromImage( image );
		ui->cardInfo->showPicture( pixmap );
		ui->infoStack->showPicture( pixmap );
	}
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
	if( action == SignatureAdd )
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

void MainWindow::selectPageIcon( PageIcon* page )
{
	ui->rightShadow->raise();
	for( auto pageIcon: { ui->signature, ui->crypto, ui->myEid } )
	{
		pageIcon->activate( pageIcon == page );
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

		QStringList firstName = QStringList()
			<< t.data( QSmartCardData::FirstName1 ).toString()
			<< t.data( QSmartCardData::FirstName2 ).toString();
		firstName.removeAll( "" );
		QStringList fullName = QStringList()
			<< firstName
			<< t.data( QSmartCardData::SurName ).toString();
		fullName.removeAll( "" );
		ui->cardInfo->update( fullName.join(" "), t.data( QSmartCardData::Id ).toString(), "Lugejas on ID kaart" );
		ui->cardInfo->setAccessibleDescription( fullName.join(" ") );

		if( t.authCert().type() & SslCertificate::EstEidType )
		{
			loadCachedPicture( t.data(QSmartCardData::Id ).toString() );
		}
		ui->infoStack->update( t );
		ui->accordion->updateInfo( smartcard );
	}
	else if( !t.card().isEmpty() )
	{
		noReader_NoCard_Loading_Event( "Loading data", true );
	}
	else if( t.card().isEmpty() && !t.readers().isEmpty() )
	{
		noReader_NoCard_Loading_Event( "Lugejas ei ole kaarti. Kontrolli, kas ID-kaart on õiget pidi lugejas." );
	}
	else
	{
		noReader_NoCard_Loading_Event( "No readers found" );
	}

	// Combo box to select the cards from
/*	ui->selector_old->hide();
	ui->selector->clear();
	ui->selector->addItems( t.cards() );
	ui->selector->setVisible( t.cards().size() > 1 );
	ui->selector->setCurrentIndex( ui->selector->findText( t.card() ) );
*/
}

void MainWindow::noReader_NoCard_Loading_Event( const QString &text, bool isLoading )
{
	ui->idSelector->hide();
	if( !isLoading )
	{
		ui->noCardInfo->show();
		ui->noCardInfo->update( text );
		ui->noCardInfo->setAccessibleDescription( text );
	}
    else
	{
		ui->noCardInfo->show();
		ui->cardInfo->update( "", "", text );
		ui->cardInfo->setAccessibleDescription( text );
		Application::setOverrideCursor( Qt::BusyCursor );
	}
	ui->infoStack->clearData();
	ui->cardInfo->clearPicture();
	ui->infoStack->clearPicture();
    ui->infoStack->hide();
	ui->accordion->hide();
	ui->accordion->updateOtherData( false );
}

// Loads picture 
void MainWindow::loadCardPhoto ()
{
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
			showWarning( XmlReader::emailErr( error.toUInt() ) );
		return;
	}

	cachePicture( smartcard->data().data(QSmartCardData::Id).toString(), image );
	QPixmap pixmap = QPixmap::fromImage( image );
	ui->cardInfo->showPicture( pixmap );
	ui->infoStack->showPicture( pixmap );
}

void MainWindow::getEmailStatus ()
{
	QByteArray buffer = sendRequest( SSLConnect::EmailInfo );

	if( buffer.isEmpty() )
		return;

	XmlReader xml( buffer );
	QString error;
	QMultiHash<QString,QPair<QString,bool> > emails = xml.readEmailStatus( error );
	quint8 code = error.toUInt();
	if( emails.isEmpty() || code > 0 )
	{
		code = code ? code : 20;
		if( code == 20 )
			ui->accordion->updateOtherData( true, XmlReader::emailErr( code ), code );
        else
			ui->accordion->updateOtherData( false, XmlReader::emailErr( code ), code );
	}
	else
	{
		QStringList text;
		for( Emails::const_iterator i = emails.constBegin(); i != emails.constEnd(); ++i )
		{
			text << QString( "%1 - %2 (%3)" )
				.arg( i.key() )
				.arg( i.value().first )
				.arg( i.value().second ? tr("active") : tr("not active") );
		}
		ui->accordion->updateOtherData( false, text.join("<br />") );
	}
}

void MainWindow::activateEmail ()
{
	QString eMail = ui->accordion->getEmail();
	if( eMail.isEmpty() )
	{
		showWarning( tr("E-mail address missing or invalid!") );
        ui->accordion->setFocusToEmail();
		return;
	}
	QByteArray buffer = sendRequest( SSLConnect::ActivateEmails, eMail );
	if( buffer.isEmpty() )
		return;
	XmlReader xml( buffer );
	QString error;
	xml.readEmailStatus( error );
	ui->accordion->updateOtherData( false, XmlReader::emailErr( error.toUInt() ) );
	return;
}

QByteArray MainWindow::sendRequest( SSLConnect::RequestType type, const QString &param )
{
/*
	switch( type )
	{
	case SSLConnect::ActivateEmails: showLoading(  tr("Activating email settings") ); break;
	case SSLConnect::EmailInfo: showLoading( tr("Loading email settings") ); break;
	case SSLConnect::MobileInfo: showLoading( tr("Requesting Mobiil-ID status") ); break;
	case SSLConnect::PictureInfo: showLoading( tr("Loading picture") ); break;
	default: showLoading( tr( "Loading data" ) ); break;
	}
*/
	if( !validateCardError( QSmartCardData::Pin1Type, type, smartcard->login( QSmartCardData::Pin1Type ) ) )
		return QByteArray();

	SSLConnect ssl;
	ssl.setToken( smartcard->data().authCert(), smartcard->key() );
	QByteArray buffer = ssl.getUrl( type, param );
	smartcard->logout();
//	updateData();
	if( !ssl.errorString().isEmpty() )
	{
		switch( type )
		{
		case SSLConnect::ActivateEmails: showWarning( tr("Failed activating email forwards."), ssl.errorString() ); break;
		case SSLConnect::EmailInfo: showWarning( tr("Failed loading email settings."), ssl.errorString() ); break;
		case SSLConnect::MobileInfo: showWarning( tr("Failed loading Mobiil-ID settings."), ssl.errorString() ); break;
		case SSLConnect::PictureInfo: showWarning( tr("Loading picture failed."), ssl.errorString() ); break;
		default: showWarning( tr("Failed to load data"), ssl.errorString() ); break;
		}
		return QByteArray();
	}
	return buffer;
}

void MainWindow::showOverlay( QWidget *parent )
{
	overlay.reset( new Overlay(parent) );
	overlay->show();
}

bool MainWindow::validateCardError( QSmartCardData::PinType type, int flags, QSmartCard::ErrorType err )
{
	// updateData();
	QSmartCardData::PinType t = flags == 1025 ? QSmartCardData::PukType : type;
	QSmartCardData td = smartcard->data();
	switch( err )
	{
	case QSmartCard::NoError: return true;
	case QSmartCard::CancelError:
#ifdef Q_OS_WIN
		if( !td.isNull() && td.isPinpad() )
		if( td.authCert().subjectInfo( "C" ) == "EE" )	// only for Estonian ID card
		{
			switch ( type )
			{
			case QSmartCardData::Pin1Type: showWarning( "PIN1 timeout" ); break;
			case QSmartCardData::Pin2Type: showWarning( "PIN2 timeout" ); break;
			case QSmartCardData::PukType: showWarning( "PUK timeout" ); break;
			}
		}
#endif
		break;
	case QSmartCard::BlockedError:
		showWarning( QString("%1 blocked").arg( QSmartCardData::typeString( t ) ) );
		pageSelected( ui->myEid );
		break;
	case QSmartCard::DifferentError:
		showWarning( QString("New %1 codes doesn't match").arg( QSmartCardData::typeString( type ) ) ); break;
	case QSmartCard::LenghtError:
		switch( type )
		{
		case QSmartCardData::Pin1Type: showWarning( "PIN1 length has to be between 4 and 12" ); break;
		case QSmartCardData::Pin2Type: showWarning( "PIN2 length has to be between 5 and 12" ); break;
		case QSmartCardData::PukType: showWarning( "PUK length has to be between 8 and 12" ); break;
		}
		break;
	case QSmartCard::OldNewPinSameError:
		showWarning( QString("Old and new %1 has to be different!").arg( QSmartCardData::typeString( type ) ) );
		break;
	case QSmartCard::ValidateError:
		showWarning( QString("Wrong %1 code.").arg( QSmartCardData::typeString( t ) ) + QString("You can try %n more time(s).").arg( smartcard->data().retryCount( t ) ));
		break;
	default:
		switch( flags )
		{
		case SSLConnect::ActivateEmails: showWarning( "Failed activating email forwards." ); break;
		case SSLConnect::EmailInfo: showWarning( "Failed loading email settings." ); break;
		case SSLConnect::MobileInfo: showWarning( "Failed loading Mobiil-ID settings." ); break;
		case SSLConnect::PictureInfo: showWarning( "Loading picture failed." ); break;
		default:
			showWarning( QString( "Changing %1 failed" ).arg( QSmartCardData::typeString( type ) ) ); break;
		}
		break;
	}
	return false;
}

void MainWindow::showWarning( const QString &msg )
{
	switch( ui->startScreen->currentIndex() )
	{
	case SignDetails:
		ui->signContainerPage->showWarningText( msg, "<a href=\"http://id.ee/\" style='color: rgb(92, 28, 28);'>Vajuta probleemi lahendamiseks</a>" );
		break;
	case CryptoDetails:
		ui->cryptoContainerPage->showWarningText( msg, "<a href=\"http://id.ee/\" style='color: rgb(92, 28, 28);'>Vajuta probleemi lahendamiseks</a>" );
		break;
	default: 
		QMessageBox::warning( this, windowTitle(), msg );
		break;
	}
}

void MainWindow::showWarning( const QString &msg, const QString &details )
{
	switch( ui->startScreen->currentIndex() )
	{
	case SignDetails:
		ui->signContainerPage->showWarningText( msg, "<a href=\"http://id.ee/\" style='color: rgb(92, 28, 28);'>Vajuta probleemi lahendamiseks</a>" );
		break;
	case CryptoDetails:
		ui->cryptoContainerPage->showWarningText( msg, "<a href=\"http://id.ee/\" style='color: rgb(92, 28, 28);'>Vajuta probleemi lahendamiseks</a>" );
		break;
	default: 
		{
			QMessageBox d( QMessageBox::Warning, windowTitle(), msg, QMessageBox::Close, qApp->activeWindow() );
			d.setWindowModality( Qt::WindowModal );
			d.setDetailedText( details );
			d.exec();
		}
		break;
	}
}
