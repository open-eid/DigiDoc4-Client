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
#include "common_enums.h"
#include "QSigner.h"
#include "sslConnect.h"
#include "Styles.h"
#include "XmlReader.h"
#include "widgets/FadeInNotification.h"

#include <common4/TokenData4.h>
#include <common/SslCertificate.h>

#include <QDebug>
#include <QtNetwork/QSslConfiguration>

using namespace ria::qdigidoc4;

MainWindow::MainWindow( QWidget *parent ) :
    QWidget( parent ),
    ui( new Ui::MainWindow )
{
    QFont openSansReg13 = Styles::instance().font( Styles::OpenSansRegular, 13 );
    QFont openSansReg14 = Styles::instance().font( Styles::OpenSansRegular, 14 );
    QFont openSansReg16 = Styles::instance().font( Styles::OpenSansRegular, 16 );
    QFont openSansReg20 = Styles::instance().font( Styles::OpenSansRegular, 20 );
    QFont openSansSBold14 = Styles::instance().font( Styles::OpenSansSemiBold, 14 ) ;

    ui->setupUi(this);
    
    ui->signature->init( "ALLKIRI", PageIcon::Style { openSansSBold14, "/images/sign_dark_38x38.png", "#ffffff", "#998B66" },
        PageIcon::Style { openSansReg14, "/images/sign_light_38x38.png", "#023664", "#ffffff" }, true );
    ui->crypto->init( "KRÜPTO", PageIcon::Style { openSansSBold14, "/images/crypto_dark_38x38.png", "#ffffff", "#998B66" },
        PageIcon::Style { openSansReg14, "/images/crypto_light_38x38.png", "#023664", "#ffffff" }, false );
    ui->myEid->init("MINU eID", PageIcon::Style { openSansSBold14, "/images/my_eid_dark_38x38.png", "#ffffff", "#998B66" },
        PageIcon::Style { openSansReg14, "/images/my_eid_light_38x38.png", "#023664", "#ffffff" }, false );

	showCardStatus();

    connect( ui->signature, &PageIcon::activated, this, &MainWindow::pageSelected );
    connect( ui->crypto, &PageIcon::activated, this, &MainWindow::pageSelected );
    connect( ui->myEid, &PageIcon::activated, this, &MainWindow::pageSelected );
    
    buttonGroup = new QButtonGroup( this );
   	buttonGroup->addButton( ui->help, HeadHelp );
   	buttonGroup->addButton( ui->settings, HeadSettings );

    ui->signIntroLabel->setFont( openSansReg20 );
    ui->signIntroButton->setFont( openSansReg16 );
    ui->cryptoIntroLabel->setFont( openSansReg20 );
    ui->cryptoIntroButton->setFont( openSansReg16 );
    
    ui->help->setFont( openSansReg13 );
    ui->settings->setFont( openSansReg13 );

    connect( ui->signIntroButton, &QPushButton::clicked, [this]() { navigateToPage(SignDetails); } );
    connect( ui->cryptoIntroButton, &QPushButton::clicked, [this]() { navigateToPage(CryptoDetails); } );
    connect( buttonGroup, static_cast<void(QButtonGroup::*)(int)>(&QButtonGroup::buttonClicked), this, &MainWindow::buttonClicked );
    connect( ui->signContainerPage, &ContainerPage::action, this, &MainWindow::onSignAction );
    connect( ui->cryptoContainerPage, &ContainerPage::action, this, &MainWindow::onCryptoAction );
    connect( ui->cardInfo, &CardInfo::thePhotoLabelClicked, this, &MainWindow::loadCardPhoto );   // To load photo
    connect( qApp->signer(), SIGNAL( signDataChanged( TokenData ) ), SLOT( showCardStatus() ) );  // To refresh ID card info

    smartcard = new QSmartCard( this );
    smartcard->start();

    ui->infoStack->update(
                "MARI",
                "MAASIKAS",
                "48405051234",
                "EESTI",
                "IK1234567",
                "<span style='color: #37a447'>Kehtiv</span> kuni 10.10.2019",
                "Kontrolli sertifikaate"
                        );

    ui->accordion->init();
}

MainWindow::~MainWindow()
{
    delete ui;
    delete buttonGroup;
    delete smartcard;
}

void MainWindow::pageSelected( PageIcon *const page )
{
    if( page != ui->signature )
    {
        ui->signature->select(false);
    } 
    else
    {
        navigateToPage(SignIntro);
    }
    if( page != ui->crypto )
    {
        ui->crypto->select(false);
    }
    else
    {
        navigateToPage(CryptoIntro);
    }
    if( page != ui->myEid )
    {
        ui->myEid->select(false);
    }
    else
    {
        navigateToPage(MyEid);
    }
}

void MainWindow::buttonClicked( int button )
{
    switch( button )
    {
    case HeadHelp:
        //QDesktopServices::openUrl( QUrl( Common::helpUrl() ) );
        QMessageBox::warning( this, "DigiDoc4 client", "Not implemented yet" );
        break;
    case HeadSettings:
        // qApp->showSettings();
        QMessageBox::warning( this, "DigiDoc4 client", "Not implemented yet" );
        break;
    default: break;
    }
}

void MainWindow::navigateToPage( Pages page )
{
    ui->startScreen->setCurrentIndex(page);

    if( page == SignDetails )
    {
        ui->signContainerPage->transition(ContainerState::UnsignedContainer);
    }
    else if( page == CryptoDetails )
    {
        ui->cryptoContainerPage->transition(ContainerState::UnencryptedContainer);
    }
}

void MainWindow::onSignAction( int action )
{
    if( action == SignatureAdd )
    {
        ui->signContainerPage->transition(ContainerState::SignedContainer);

        FadeInNotification* notification = new FadeInNotification( this, "#ffffff", "#53c964" );
        notification->start( "Konteiner on edukalt allkirjastatud!", 750, 1500, 600 );
    }
    else if( action == FileRemove )
    {

    }
}

void MainWindow::onCryptoAction( int action )
{
    if( action == ContainerCancel )
    {
        ui->cryptoContainerPage->transition(ContainerState::EncryptedContainer);

        FadeInNotification* notification = new FadeInNotification( this, "#ffffff", "#53c964" );
        notification->start( "Krüpteerimine õnnestus!", 750, 1500, 600 );
    }
    else if( action == FileRemove )
    {

    }
}

void MainWindow::loadCardPhoto ()
{
    loadPicture();
}

void MainWindow::showCardStatus()
{
	Application::restoreOverrideCursor();
	TokenData4 t = qApp->signer()->tokensign();
	if( !t.card().isEmpty() && !t.cert().isNull() )
	{
        ui->idSelector->show();
        ui->noCardInfo->hide();
        ui->cardInfo->update( t.GetName(), t.GetCode(), "Lugejas on ID kaart" );
		ui->cardInfo->setAccessibleDescription( t.toAccessible() );
	}
	else if( !t.card().isEmpty() )
	{
        ui->idSelector->show();
        ui->noCardInfo->hide();
        ui->cardInfo->update( "", "", "Loading data" );
		ui->cardInfo->setAccessibleDescription( "Loading data" );
		Application::setOverrideCursor( Qt::BusyCursor );
	}
	else if( t.card().isEmpty() && !t.readers().isEmpty() )
	{
        ui->idSelector->hide();
        ui->noCardInfo->show();
        ui->noCardInfo->update( "Lugejas ei ole kaarti. Kontrolli, kas ID-kaart on õiget pidi lugejas." );
		ui->noCardInfo->setAccessibleDescription( "Lugejas ei ole kaarti. Kontrolli, kas ID-kaart on õiget pidi lugejas." );
	}
	else
	{
        ui->idSelector->hide();
        ui->noCardInfo->show();
        ui->noCardInfo->update( "No readers found" );
		ui->noCardInfo->setAccessibleDescription( "No readers found" );
	}
/*
    // Combo box to select the cards from
	cards->clear();
	cards->addItems( t.cards() );
	cards->setVisible( t.cards().size() > 1 );
	cards->setCurrentIndex( cards->findText( t.card() ) );
	qDeleteAll( cardsGroup->actions() );
	for( int i = 0; i < t.cards().size(); ++i )
	{
		QAction *a = cardsGroup->addAction( new QAction( cardsGroup ) );
		a->setData( t.cards().at( i ) );
		a->setShortcut( Qt::CTRL + (Qt::Key_1 + i) );
	}
	addActions( cardsGroup->actions() );

	enableSign();
*/
}

// Loads picture so far. 
// Somehow in test environment it seems does not load correct data, so picture is not shown. Oleg.
void MainWindow::loadPicture()
{
    SSLConnect::RequestType type = SSLConnect::PictureInfo;
    
	if( !validateCardError( QSmartCardData::Pin1Type, type, smartcard->login( QSmartCardData::Pin1Type ) ) )
		return;

    const QString &param = QString();
	SSLConnect ssl;
	ssl.setToken( smartcard->data().authCert(), smartcard->key() );
	QByteArray buffer = ssl.getUrl( type, param );
    int size = buffer.size();
	smartcard->logout();

 //	updateData();
	if( !ssl.errorString().isEmpty() )
	{
		switch( type )
		{
		case SSLConnect::ActivateEmails: showWarning( "Failed activating email forwards.", ssl.errorString() ); break;
		case SSLConnect::EmailInfo: showWarning( "Failed loading email settings.", ssl.errorString() ); break;
		case SSLConnect::MobileInfo: showWarning( "Failed loading Mobiil-ID settings.", ssl.errorString() ); break;
		case SSLConnect::PictureInfo: showWarning( "Loading picture failed.", ssl.errorString() ); break;
		default: showWarning( "Failed to load data", ssl.errorString() ); break;
		}
		return ;
	}
	if( buffer.isEmpty() )
		return;

	if( ui->cardInfo->loadPicture(buffer) )
	{
		XmlReader xml( buffer );
		QString error;
		xml.readEmailStatus( error );
		if( !error.isEmpty() )
			showWarning( XmlReader::emailErr( error.toUInt() ) );
		return;
	}
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
{ QMessageBox::warning( this, windowTitle(), msg ); }

void MainWindow::showWarning( const QString &msg, const QString &details )
{
	QMessageBox d( QMessageBox::Warning, windowTitle(), msg, QMessageBox::Close, qApp->activeWindow() );
	d.setWindowModality( Qt::WindowModal );
	d.setDetailedText( details );
	d.exec();
}

