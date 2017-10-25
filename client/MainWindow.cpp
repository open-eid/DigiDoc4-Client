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

#include "AccessCert.h"
#include "Application.h"
#include "Colors.h"
#include "DigiDoc.h"
#include "FileDialog.h"
#include "QPCSC.h"
#include "QSigner.h"
#include "Styles.h"
#include "XmlReader.h"
#include "crypto/CryptoDoc.h"
#include "effects/FadeInNotification.h"
#include "effects/ButtonHoverFilter.h"
#include "dialogs/FirstRun.h"
#include "dialogs/MobileProgress.h"
#include "dialogs/SettingsDialog.h"
#include "dialogs/WaitDialog.h"
#include "dialogs/WarningDialog.h"
#include "util/FileUtil.h"
#ifdef Q_OS_WIN
#include "CertStore.h"
#endif

#include <common/DateTime.h>
#include <common/Settings.h>
#include <common/SslCertificate.h>
#include <common/TokenData.h>

#include <QDebug>
#include <QFileDialog>
#include <QMessageBox>
#include <QMimeData>
#include <QSvgWidget>
#include <QtCore/QUrlQuery>
#include <QtGui/QDesktopServices>

using namespace ria::qdigidoc4;
using namespace ria::qdigidoc4::colors;

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

	QSvgWidget* coatOfArms = new QSvgWidget(ui->logo);
	coatOfArms->load( QString( ":/images/Logo_small.svg" ) );
	coatOfArms->resize( 80, 32 );
	coatOfArms->move( 15, 17 );
	ui->signature->init( Pages::SignIntro, ui->signatureShadow, true );
	ui->crypto->init( Pages::CryptoIntro, ui->cryptoShadow, false );
	ui->myEid->init( Pages::MyEid, ui->myEidShadow, false );

	connect( ui->signature, &PageIcon::activated, this, &MainWindow::pageSelected );
	connect( ui->crypto, &PageIcon::activated, this, &MainWindow::pageSelected );
	connect( ui->myEid, &PageIcon::activated, this, &MainWindow::pageSelected );

	selector = new DropdownButton(":/images/arrow_down.svg", ":/images/arrow_down_selected.svg", ui->idSelector);
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

	setAcceptDrops( true );
	smartcard = new QSmartCard( this );
	// Refresh ID card info in card widget
	connect( smartcard, &QSmartCard::dataChanged, this, &MainWindow::showCardStatus );
	// Refresh card info in card pop-up menu
	connect( smartcard, &QSmartCard::dataLoaded, this, &MainWindow::updateCardData );
	// Show card pop-up menu
	connect( selector, &DropdownButton::dropdown, this, &MainWindow::showCardMenu );
	smartcard->start();

	ui->accordion->init();

	hideWarningArea();

	connect( ui->signIntroButton, &QPushButton::clicked, this, &MainWindow::openContainer );
	connect( ui->cryptoIntroButton, &QPushButton::clicked, this, &MainWindow::openContainer );
	connect( buttonGroup, static_cast<void(QButtonGroup::*)(int)>(&QButtonGroup::buttonClicked), this, &MainWindow::buttonClicked );
	connect( ui->signContainerPage, &ContainerPage::action, this, &MainWindow::onSignAction );
	connect( ui->signContainerPage, &ContainerPage::removed, this, &MainWindow::removeSignature );
	connect( ui->cryptoContainerPage, &ContainerPage::action, this, &MainWindow::onCryptoAction );
	connect( ui->cryptoContainerPage, &ContainerPage::removed, this, &MainWindow::removeAddress );
	connect( ui->accordion, &Accordion::checkEMail, this, &MainWindow::getEmailStatus );   // To check e-mail
	connect( ui->accordion, &Accordion::activateEMail, this, &MainWindow::activateEmail );   // To activate e-mail
	connect( ui->infoStack, &InfoStack::photoClicked, this, &MainWindow::photoClicked );
	connect( ui->cardInfo, &CardWidget::photoClicked, this, &MainWindow::photoClicked );   // To load photo
	connect( ui->cardInfo, &CardWidget::selected, this, [this]() { if( selector ) selector->press(); } );
	connect( ui->accordion, &Accordion::checkOtherEID, this, &MainWindow::getOtherEID );

	showCardStatus();
	connect( ui->accordion, &Accordion::changePin1Clicked, this, &MainWindow::changePin1Clicked );
	connect( ui->accordion, &Accordion::changePin2Clicked, this, &MainWindow::changePin2Clicked );
	connect( ui->accordion, &Accordion::changePukClicked, this, &MainWindow::changePukClicked );
	connect( ui->accordion, &Accordion::certDetailsClicked, this, &MainWindow::certDetailsClicked );

	connect( ui->warningAction, &QLabel::linkActivated, this, &MainWindow::updateCertificate );
}

MainWindow::~MainWindow()
{
	delete ui;
	delete buttonGroup;
	delete smartcard;
}

void MainWindow::pageSelected( PageIcon *const page )
{
	navigateToPage( page->getType() );
}

void MainWindow::buttonClicked( int button )
{
	switch( button )
	{
	case HeadHelp:
	{
		FirstRun dlg(this);
		dlg.exec();
		break;
	}
	case HeadSettings:
	{
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

ContainerState MainWindow::currentState()
{
	auto current = ui->startScreen->currentIndex();	
	bool cryptoPage = (current == CryptoIntro || current == CryptoDetails);

	if(cryptoPage && cryptoDoc)
	{
		return cryptoDoc->state();
	}

	if(digiDoc)
	{
		return digiDoc->state();
	}

	return ContainerState::Uninitialized;
}

bool MainWindow::decrypt()
{
	if(!cryptoDoc)
		return false;

	WaitDialog waitDialog(qApp->activeWindow());
	waitDialog.open();

	return cryptoDoc->decrypt();
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

bool MainWindow::encrypt()
{
	if(!cryptoDoc)
		return false;

	WaitDialog waitDialog(qApp->activeWindow());
	waitDialog.open();

	return cryptoDoc->encrypt();
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

void MainWindow::mousePressEvent(QMouseEvent *event)
{
	if( ui->warning->underMouse() )
	{
		if( ui->warning->property("updateCertificateEnabled").toBool() )
			showUpdateCertWarning();
		else
			hideWarningArea();
	}
}

void MainWindow::navigateToPage( Pages page, const QStringList &files, bool create )
{
	bool navigate = true;	
	if(page == SignDetails)
	{
		navigate = false;
		std::unique_ptr<DigiDoc> signatureContainer(new DigiDoc(this));
		if(create)
		{
			QString ext = Settings(qApp->applicationName()).value("Client/Type", "bdoc").toString();
			QString filename = FileUtil::createFile(files[0], QString(".%1").arg(ext), tr("signature container"));
			if(!filename.isNull())
			{
				signatureContainer->create(filename);
				for(auto file: files)
					signatureContainer->documentModel()->addFile(file);
				navigate = true;
			}
		}
		else if(signatureContainer->open(files[0]))
		{
			navigate = true;
		}
		if(navigate)
		{
			delete digiDoc;
			digiDoc = signatureContainer.release();
			connect(digiDoc, &DigiDoc::operation, this, &MainWindow::operation);
			ui->signContainerPage->transition(digiDoc);
		}
	}
	else if(page == CryptoDetails)
	{
		navigate = false;
		std::unique_ptr<CryptoDoc> cryptoContainer(new CryptoDoc(this));

		if(create)
		{
			QString filename = FileUtil::createFile(files[0], ".cdoc", tr("crypto container"));
			if(!filename.isNull())
			{
				cryptoContainer->clear(filename);
				for(auto file: files)
					cryptoContainer->documentModel()->addFile(file);
				auto cardData = smartcard->data();
				if(!cardData.isNull())
				{
					cryptoContainer->addKey(CKey(cardData.authCert()));
				}
				navigate = true;
			}
		}
		else if(cryptoContainer->open(files[0]))
		{
			navigate = true;
		}
		if(navigate)
		{
			delete cryptoDoc;
			cryptoDoc = cryptoContainer.release();
			ui->cryptoContainerPage->transition(cryptoDoc);
		}
	}
	else
	{
		if(cryptoDoc)
		{
			delete cryptoDoc;
			cryptoDoc = nullptr;
		}
		if(digiDoc)
		{
			delete digiDoc;
			digiDoc = nullptr;
		}
	}

	if(navigate)
	{
		selectPageIcon( page < CryptoIntro ? ui->signature : (page == MyEid ? ui->myEid : ui->crypto));
		ui->startScreen->setCurrentIndex(page);
	}
}

void MainWindow::onSignAction(int action, const QString &idCode, const QString &phoneNumber)
{
	switch(action)
	{
	case SignatureAdd:
		sign();
		break;
	case SignatureMobile:
		signMobile(idCode, phoneNumber);
		break;
	case ContainerCancel:
		navigateToPage(Pages::SignIntro);
		break;
	case ContainerSave:
		save();
		ui->signContainerPage->transition(digiDoc);
		break;
	case ContainerEmail:
		if(digiDoc)
			containerToEmail(digiDoc->fileName());
		break;
	case ContainerLocation:
		if(digiDoc)
			moveSignatureContainer();
		break;
	case ContainerNavigate:
		if(digiDoc)
			browseOnDisk(digiDoc->fileName());
		break;
	case ContainerEncrypt:
		if(digiDoc)
			convertToCDoc();
		break;
	default:
		break;
	}
}

void MainWindow::convertToBDoc()
{
	QString ext = Settings(qApp->applicationName()).value("Client/Type", "bdoc").toString();
	QString filename = FileUtil::createFile(cryptoDoc->fileName(), QString(".%1").arg(ext), tr("signature container"));
	if(filename.isNull())
		return;

	std::unique_ptr<DigiDoc> signatureContainer(new DigiDoc(this));
	signatureContainer->create(filename);
	signatureContainer->documentModel()->addTempFiles(cryptoDoc->documentModel()->tempFiles());
	
	delete digiDoc;
	digiDoc = signatureContainer.release();
	connect(digiDoc, &DigiDoc::operation, this, &MainWindow::operation);
	delete cryptoDoc;
	cryptoDoc = nullptr;

	selectPageIcon(ui->signature);
	ui->startScreen->setCurrentIndex(CryptoDetails);

	FadeInNotification* notification = new FadeInNotification( this, WHITE, MANTIS, 110 );
	notification->start( tr("Konverteeritud allkirjadokumendiks!"), 750, 1500, 600 );
}

void MainWindow::convertToCDoc()
{
	QString filename = FileUtil::createFile(digiDoc->fileName(), ".cdoc", tr("crypto container"));
	if(filename.isNull())
		return;

	std::unique_ptr<CryptoDoc> cryptoContainer(new CryptoDoc(this));
	cryptoContainer->clear(filename);

	// If signed, add whole signed document to cryptocontainer; otherwise content only
	if(digiDoc->state() == SignedContainer)
		cryptoContainer->documentModel()->addFile(digiDoc->fileName());
	else
		cryptoContainer->documentModel()->addTempFiles(digiDoc->documentModel()->tempFiles());

	auto cardData = smartcard->data();
	if(!cardData.isNull())
		cryptoContainer->addKey(CKey(cardData.authCert()));

	delete cryptoDoc;
	cryptoDoc = cryptoContainer.release();
	delete digiDoc;
	digiDoc = nullptr;
	ui->cryptoContainerPage->transition(cryptoDoc);
	selectPageIcon(ui->crypto);
	ui->startScreen->setCurrentIndex(CryptoDetails);

	FadeInNotification* notification = new FadeInNotification( this, WHITE, MANTIS, 110 );
	notification->start( tr("Konverteeritud krüptokontaineriks!"), 750, 1500, 600 );
}

void MainWindow::moveCryptoContainer()
{
	const QFileInfo f(cryptoDoc->fileName());
	QString to = FileDialog::getSaveFileName(this, tr("Move file"), cryptoDoc->fileName(), f.suffix());
	if(!to.isNull() && cryptoDoc->move(to))
		ui->cryptoContainerPage->moved(to);
}

void MainWindow::moveSignatureContainer()
{
	const QFileInfo f(digiDoc->fileName());
	QString to = FileDialog::getSaveFileName(this, tr("Move file"), digiDoc->fileName(), f.suffix());
	if(!to.isNull() && digiDoc->move(to))
		ui->signContainerPage->moved(to);
}

void MainWindow::onCryptoAction(int action, const QString &id, const QString &phone)
{
	switch(action)
	{
	case ContainerCancel:
		navigateToPage(Pages::CryptoIntro);
		break;
	case DecryptContainer:
		if(decrypt())
		{
			ui->cryptoContainerPage->transition(cryptoDoc);

			FadeInNotification* notification = new FadeInNotification( this, WHITE, MANTIS, 110 );
			notification->start( "Dekrüpteerimine õnnestus!", 750, 1500, 600 );
		}
		break;
	case EncryptContainer:
		if(encrypt())
		{
			ui->cryptoContainerPage->transition(cryptoDoc);

			FadeInNotification* notification = new FadeInNotification( this, WHITE, MANTIS, 110 );
			notification->start( "Krüpteerimine õnnestus!", 750, 1500, 600 );
		}
		break;
	case ContainerEmail:
		if( cryptoDoc )
			containerToEmail( cryptoDoc->fileName() );
		break;
	case ContainerLocation:
		if(cryptoDoc)
			moveCryptoContainer();
		break;
	case ContainerNavigate:
		if( cryptoDoc )
			browseOnDisk( cryptoDoc->fileName() );
		break;
	}
}

void MainWindow::openFiles(const QStringList files)
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
	2.1 if UnsignedContainer | UnsignedSavedContainer | UnencryptedContainer
		- Add file to files to be signed/encrypted
	2.3 else if SignedContainer
		- ask if should be added or handle open
	2.4 else if EncryptedContainer || DecryptedContainer
			- ask if should be closed and handle open 
*/
	auto current = ui->startScreen->currentIndex();
	ContainerState state = currentState();
	Pages page = SignDetails;
	bool create = true;
	switch(state)
	{
	case Uninitialized:
	// Case 1.
		if(files.size() == 1)
		{
			auto fileType = FileUtil::detect(files[0]);
			if(fileType == CryptoDocument)
			{
				page = CryptoDetails;
				create = false;
			}
			else if(fileType == SignatureDocument)
			{
				page = SignDetails;
				create = false;
			}
			else if(current == CryptoIntro)
			{
				page = CryptoDetails;
			}
		}
		else if(current == CryptoIntro)
		{
			page = CryptoDetails;
		}
		break;
	case ContainerState::UnsignedContainer:
	case ContainerState::UnsignedSavedContainer:
	case ContainerState::UnencryptedContainer:
	{
		page = (state == ContainerState::UnencryptedContainer) ? CryptoDetails : SignDetails;
		DocumentModel* model = (current == CryptoDetails) ?
			cryptoDoc->documentModel() : digiDoc->documentModel();
		for(auto file: files)
			model->addFile(file);
		create = false;
		break;
	}
	case ContainerState::EncryptedContainer:
	case ContainerState::DecryptedContainer:
		// TODO: new container???
		create = false;
		break;
	default:
		// TODO: new container???
		page = SignDetails;
		create = false;
		break;
	}

	if(create || current != page)
		navigateToPage( page, files, create );
}

void MainWindow::openContainer()
{
	QStringList files = FileDialog::getOpenFileNames(this, tr("Select documents"));
	if(!files.isEmpty())
		openFiles(files);
}

void MainWindow::operation(int op, bool started)
{
	qDebug() << "Op " << op << (started ? " started" : " ended");
}

void MainWindow::resizeEvent( QResizeEvent *event )
{
	ui->version->move( ui->version->geometry().x(), ui->leftBar->height() - ui->version->height() - 11 );
}

bool MainWindow::save()
{
	if(digiDoc)
	{
		if( !FileDialog::fileIsWritable(digiDoc->fileName()) &&
			QMessageBox::Yes == QMessageBox::warning( this, tr("DigiDoc4 client"),
				tr("Cannot alter container %1. Save different location?")
					.arg(digiDoc->fileName().normalized(QString::NormalizationForm_C)),
				QMessageBox::Yes|QMessageBox::No, QMessageBox::Yes))
		{
			QString file = selectFile(digiDoc->fileName(), true);
			if( !file.isEmpty() )
			{
				return digiDoc->save(file);
			}
		}
		return digiDoc->save();
	}

	return false;
}

QString MainWindow::selectFile( const QString &filename, bool fixedExt )
{
	static const QString adoc = tr("Documents (%1)").arg( "*.adoc" );
	static const QString bdoc = tr("Documents (%1)").arg( "*.bdoc" );
	static const QString edoc = tr("Documents (%1)").arg( "*.edoc" );
	static const QString asic = tr("Documents (%1)").arg( "*.asice *.sce" );
	const QString ext = QFileInfo( filename ).suffix().toLower();
	QStringList exts;
	QString active;
	if( fixedExt )
	{
		if( ext == "bdoc" ) exts << bdoc;
		if( ext == "asic" || ext == "sce" ) exts << asic;
		if( ext == "edoc" ) exts << edoc;
		if( ext == "adoc" ) exts << adoc;
	}
	else
	{
		exts << bdoc << asic << edoc << adoc;
		if( ext == "bdoc" ) active = bdoc;
		if( ext == "asice" || ext == "sce" ) active = asic;
		if( ext == "edoc" ) active = edoc;
		if( ext == "adoc" ) active = adoc;
	}

	return FileDialog::getSaveFileName( this, tr("Save file"), filename, exts.join(";;"), &active );
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
		
		QSharedPointer<const QCardInfo> cardInfo(new QCardInfo(t));
		ui->cardInfo->update(cardInfo);
		emit ui->signContainerPage->cardChanged(cardInfo->id);
		emit ui->cryptoContainerPage->cardChanged(cardInfo->id);
		
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

		isUpdateCertificateNeeded();
		ui->myEid->updateCertNeededIcon( ui->warning->property("updateCertificateEnabled").toBool() );
		if( ui->warning->property("updateCertificateEnabled").toBool() )
			showUpdateCertWarning();

		showIdCardAlerts( t );
	}
	else
	{
		emit ui->signContainerPage->cardChanged();
		emit ui->cryptoContainerPage->cardChanged();
		
		if ( !QPCSC::instance().serviceRunning() )
			noReader_NoCard_Loading_Event( "PCSC service is not running" );
		else if ( t.readers().isEmpty() )
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

void MainWindow::showOverlay( QWidget *parent )
{
	overlay.reset( new Overlay(parent) );
	overlay->show();
}

bool MainWindow::sign()
{
	AccessCert access(this);
	if( !access.validate() )
		return false;

	QString role = Settings(qApp->applicationName()).value( "Client/Role" ).toString();
	QString city = Settings(qApp->applicationName()).value( "Client/City" ).toString();
	QString state = Settings(qApp->applicationName()).value( "Client/State" ).toString();
	QString country = Settings(qApp->applicationName()).value( "Client/Country" ).toString();
	QString zip = Settings(qApp->applicationName()).value( "Client/Zip" ).toString();
	if(digiDoc->sign(city, state, zip, country, role, ""))
	{
		access.increment();
		save();
		ui->signContainerPage->transition(digiDoc);

		FadeInNotification* notification = new FadeInNotification( this, WHITE, MANTIS, 110 );
		notification->start( "Konteiner on edukalt allkirjastatud!", 750, 1500, 600 );
		return true;
	}

	return false;
}

bool MainWindow::signMobile(const QString &idCode, const QString &phoneNumber)
{
	AccessCert access(this);
	if( !access.validate() )
		return false;

	QString role = Settings(qApp->applicationName()).value( "Client/Role" ).toString();
	QString city = Settings(qApp->applicationName()).value( "Client/City" ).toString();
	QString state = Settings(qApp->applicationName()).value( "Client/State" ).toString();
	QString country = Settings(qApp->applicationName()).value( "Client/Country" ).toString();
	QString zip = Settings(qApp->applicationName()).value( "Client/Zip" ).toString();
	MobileProgress m(this);
	m.setSignatureInfo(city, state, zip, country, role);
	m.sign(digiDoc, idCode, phoneNumber);
	if( !m.exec() || !digiDoc->addSignature( m.signature() ) )
		return false;

	access.increment();
	save();

	ui->signContainerPage->transition(digiDoc);

	FadeInNotification* notification = new FadeInNotification( this, WHITE, MANTIS, 110 );
	notification->start( "Konteiner on edukalt allkirjastatud!", 750, 1500, 600 );
	return true;
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
	ui->accordion->clearOtherEID();
	ui->myEid->invalidCertIcon( false );
	ui->myEid->pinIsBlockedIcon( false );
	ui->myEid->updateCertNeededIcon( false );
	ui->warning->setProperty( "updateCertificateEnabled", false );
	hideWarningArea();
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

void MainWindow::removeAddress(int index)
{
	if(cryptoDoc)
	{
		cryptoDoc->removeKey(index);
		ui->cryptoContainerPage->transition(cryptoDoc);
	}
}

void MainWindow::removeSignature(int index)
{
	if(digiDoc)
	{
		digiDoc->removeSignature(index);
		save();
		ui->signContainerPage->transition(digiDoc);
	}
}

void MainWindow::savePhoto( const QPixmap *photo )
{
	QString fileName = QFileDialog::getSaveFileName(this,
		"Salvesta foto", "",
		"Foto (*.jpg);;All Files (*)");

	photo->save( fileName, "JPEG", 100 );
}

void MainWindow::showWarning( const QString &msg, const QString &details, bool extLink )
{
	ui->topBarShadow->setStyleSheet("background: qlineargradient(x1: 0, y1: 0, x2: 0, y2: 1, stop: 0 #b5aa92, stop: 1 #F8DDA7); \nborder: none;");	
	ui->warning->show();
	ui->warningText->setText( msg );
	ui->warningAction->setText( details );
	ui->warningAction->setTextFormat( Qt::RichText );
	ui->warningAction->setTextInteractionFlags( Qt::TextBrowserInteraction );
	ui->warningAction->setOpenExternalLinks( extLink );
}

void MainWindow::containerToEmail( const QString &fileName )
{
	QUrlQuery q;
	QUrl url;

	if ( !QFileInfo( fileName ).exists() )
		return;
	q.addQueryItem( "subject", QFileInfo( fileName ).fileName() );
	q.addQueryItem( "attachment", QFileInfo( fileName ).absoluteFilePath() );
	url.setScheme( "mailto" );
	url.setQuery(q);
	QDesktopServices::openUrl( url );
}

void MainWindow::browseOnDisk( const QString &fileName )
{
	if ( !QFileInfo( fileName ).exists() )
		return;
	QUrl url = QUrl::fromLocalFile( fileName );
	url.setScheme( "browse" );
	QDesktopServices::openUrl( url );
}

void MainWindow::showUpdateCertWarning()
{
	showWarning("Kaardi sertifikaadid vajavad uuendamist. Uuendamine võtab aega 2-10 minutit ning eeldab toimivat internetiühendust. Kaarti ei tohi lugejast enne uuenduse lõppu välja võtta.",
		"<a href='#update-Certificate'><span style='color:rgb(53, 55, 57)'>Uuenda</span></a>");
}

void MainWindow::showIdCardAlerts(const QSmartCardData& t)
{
	if(smartcard->property( "lastcard" ).toString() != t.card() &&
		t.version() == QSmartCardData::VER_3_4 &&
		(!t.authCert().validateEncoding() || !t.signCert().validateEncoding()))
	{
		qApp->showWarning( tr(
			"Your ID-card certificates cannot be renewed starting from 01.07.2017. Your document is still valid until "
			"its expiring date and it can be used to login to e-services and give digital signatures. If there are "
			"problems using Your ID-card in e-services please contact ID-card helpdesk by phone (+372) 677 3377 or "
			"visit Police and Border Guard Board service point.<br /><br />"
			"<a href=\"http://id.ee/?id=30519&read=38011\">More info</a>") );
	}
	smartcard->setProperty("lastcard", t.card());

#ifdef Q_OS_WIN
	CertStore store;
	if( !Settings().value( "Utility/showRegCert", false ).toBool() ||
		(!store.find( t.authCert() ) || !store.find( t.signCert() )) &&
		QMessageBox::question( this, tr( "Certificate store" ),
			tr( "Certificate is not registered in the certificate store. Register now?" ),
			QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes ) == QMessageBox::Yes )
	{
		QString personalCode = t.authCert().subjectInfo( "serialNumber" );
		for( const SslCertificate &c: store.list())
		{
			if( c.subjectInfo( "serialNumber" ) == personalCode )
				store.remove( c );
		}
		store.add( t.authCert(), t.card() );
		store.add( t.signCert(), t.card() );
	}
#endif
}
