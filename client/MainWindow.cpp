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
#include "CheckConnection.h"
#include "Colors.h"
#include "DigiDoc.h"
#include "FileDialog.h"
#include "QPCSC.h"
#include "QSigner.h"
#include "Styles.h"
#include "XmlReader.h"
#ifdef Q_OS_WIN
#include "CertStore.h"
#endif
#include "crypto/CryptoDoc.h"
#include "effects/FadeInNotification.h"
#include "effects/ButtonHoverFilter.h"
#include "dialogs/FirstRun.h"
#include "dialogs/MobileProgress.h"
#include "dialogs/SettingsDialog.h"
#include "dialogs/WaitDialog.h"
#include "dialogs/WarningDialog.h"
#include "util/FileUtil.h"
#include "widgets/WarningItem.h"
#include "widgets/WarningRibbon.h"
#include "widgets/VerifyCert.h"

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
	ui->version->setText( tr("Ver. ") + qApp->applicationVersion() );

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

	selector = new DropdownButton(":/images/arrow_down.svg", ":/images/arrow_down_selected.svg", ui->selector);
	selector->hide();
	selector->resize( 12, 6 );
	selector->move( 9, 32 );
	selector->setCursor( Qt::PointingHandCursor );
	ui->help->installEventFilter( new ButtonHoverFilter( ":/images/icon_Abi.svg", ":/images/icon_Abi_hover.svg", this ) );
	ui->settings->installEventFilter( new ButtonHoverFilter( ":/images/icon_Seaded.svg", ":/images/icon_Seaded_hover.svg", this ) );
	buttonGroup = new QButtonGroup( this );
	buttonGroup->addButton( ui->help, HeadHelp );
	buttonGroup->addButton( ui->settings, HeadSettings );

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

	connect(ui->signIntroButton, &QPushButton::clicked, this, &MainWindow::openContainer);
	connect(ui->cryptoIntroButton, &QPushButton::clicked, this, &MainWindow::openContainer);
	connect(buttonGroup, static_cast<void(QButtonGroup::*)(int)>(&QButtonGroup::buttonClicked), this, &MainWindow::buttonClicked);
	connect(ui->signContainerPage, &ContainerPage::action, this, &MainWindow::onSignAction);
	connect(ui->signContainerPage, &ContainerPage::addFiles, this, &MainWindow::openFiles);
	connect(ui->signContainerPage, &ContainerPage::fileRemoved, this, &MainWindow::removeSignatureFile);
	connect(ui->signContainerPage, &ContainerPage::removed, this, &MainWindow::removeSignature);

	connect(ui->cryptoContainerPage, &ContainerPage::action, this, &MainWindow::onCryptoAction);
	connect(ui->cryptoContainerPage, &ContainerPage::addFiles, this, &MainWindow::openFiles);
	connect(ui->cryptoContainerPage, &ContainerPage::fileRemoved, this, &MainWindow::removeCryptoFile);
	connect(ui->cryptoContainerPage, &ContainerPage::keysSelected, this, &MainWindow::updateKeys);
	connect(ui->cryptoContainerPage, &ContainerPage::removed, this, &MainWindow::removeAddress);

	connect(ui->accordion, &Accordion::checkEMail, this, &MainWindow::getEmailStatus);   // To check e-mail
	connect(ui->accordion, &Accordion::activateEMail, this, &MainWindow::activateEmail);   // To activate e-mail
	connect(ui->infoStack, &InfoStack::photoClicked, this, &MainWindow::photoClicked);
	connect(ui->cardInfo, &CardWidget::photoClicked, this, &MainWindow::photoClicked);   // To load photo
	connect(ui->cardInfo, &CardWidget::selected, this, [this]() { if( selector ) selector->press(); });
	connect(ui->accordion, &Accordion::checkOtherEID, this, &MainWindow::getOtherEID);

	showCardStatus();
	connect(ui->accordion, &Accordion::changePin1Clicked, this, &MainWindow::changePin1Clicked);
	connect(ui->accordion, &Accordion::changePin2Clicked, this, &MainWindow::changePin2Clicked);
	connect(ui->accordion, &Accordion::changePukClicked, this, &MainWindow::changePukClicked);
	connect(ui->accordion, &Accordion::certDetailsClicked, this, &MainWindow::certDetailsClicked);
}

MainWindow::~MainWindow()
{
	delete ui;
	delete buttonGroup;
	delete smartcard;
}

void MainWindow::pageSelected( PageIcon *const page )
{
	// Stay in current view if same page icon clicked
	auto current = ui->startScreen->currentIndex();
	bool navigate = false;
	switch(current)
	{
	case SignIntro:
	case SignDetails:
		navigate = (page->getType() != SignIntro);
		break;
	case CryptoIntro:
	case CryptoDetails:
		navigate = (page->getType() != CryptoIntro);
		break;
	default:
		navigate = (page->getType() != current);
		break;
	}

	Pages toPage = page->getType();
	if(toPage == SignIntro)
	{
		if(digiDoc)
		{
			navigate = false;
			selectPageIcon(ui->signature);
			ui->startScreen->setCurrentIndex(SignDetails);
			updateWarnings();
		}
	}
	else if(toPage == CryptoIntro)
	{
		if(cryptoDoc)
		{
			navigate = false;
			selectPageIcon(ui->crypto);
			ui->startScreen->setCurrentIndex(CryptoDetails);
			updateWarnings();
		}
	}

	if(navigate)
		navigateToPage(toPage);
}

void MainWindow::buttonClicked( int button )
{
	switch( button )
	{
	case HeadHelp:
	{
		FirstRun dlg(this);

		connect(&dlg, &FirstRun::langChanged, this,
				[this](const QString& lang )
				{
					qApp->loadTranslation( lang );
					ui->retranslateUi(this);
				}
		);
		dlg.exec();

		break;
	}
	case HeadSettings:
	{
		showSettings(SettingsDialog::GeneralSettings);
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

	if(current == CryptoIntro || current == CryptoDetails)
	{
		if(cryptoDoc)
			return cryptoDoc->state();
	}
	else if(digiDoc)
	{
		return digiDoc->state();
	}

	return ContainerState::Uninitialized;
}

bool MainWindow::decrypt()
{
	if(!cryptoDoc)
		return false;

	WaitDialog waitDialog(this);
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

	if (mimeData->hasUrls())
	{
		for( auto url: mimeData->urls())
		{
			if (url.scheme() == "file" )
			{
				files << url.toLocalFile();
			}
		}
	}
	else
	{
		showNotification( tr("Unrecognized data") );
	}
	event->acceptProposedAction();
	clearOverlay();

	if( !files.isEmpty() )
	{
		openFiles( files );
	}
}

void MainWindow::changeEvent(QEvent* event)
{
	if (event->type() == QEvent::LanguageChange)
	{
		ui->retranslateUi(this);

		ui->version->setText( tr("Ver. ") + qApp->applicationVersion() );
	}

	QWidget::changeEvent(event);
}

bool MainWindow::checkConnection()
{
	CheckConnection connection;
	if(!connection.check("http://ocsp.sk.ee"))
	{
		qApp->showWarning(connection.errorString(), connection.errorDetails());
		switch(connection.error())
		{
		case QNetworkReply::ProxyConnectionRefusedError:
		case QNetworkReply::ProxyConnectionClosedError:
		case QNetworkReply::ProxyNotFoundError:
		case QNetworkReply::ProxyTimeoutError:
		case QNetworkReply::ProxyAuthenticationRequiredError:
		case QNetworkReply::UnknownProxyError:
			showSettings(SettingsDialog::NetworkSettings);
		default:
			break;
		}
		return false;
	}

	return true;
}

void MainWindow::clearWarning(const char* warningIdent)
{
	for(auto warning: warnings)
	{
		if(warning->property(warningIdent).toBool())
		{
			closeWarning(warning, true);
			break;
		}
	}

	updateWarnings();
}

void MainWindow::closeEvent(QCloseEvent *event)
{
	resetDigiDoc();
}

bool MainWindow::closeWarning(WarningItem *warning, bool force)
{
	if(force || !warning->property(UPDATE_CERT_WARNING).toBool())
	{
		warnings.removeOne(warning);
		warning->close();
		delete warning;
		updateWarnings();

		return true;
	}

	return false;
}

void MainWindow::closeWarnings(int page)
{
	for(auto warning: warnings)
	{
		if(warning->page() == page || page == -1)
			closeWarning(warning);
	}

	updateWarnings();
}

bool MainWindow::encrypt()
{
	if(!cryptoDoc)
		return false;

	WaitDialog waitDialog(this);
	waitDialog.setText(tr("Encrypting"));
	waitDialog.open();

	return cryptoDoc->encrypt();
}

void MainWindow::hideCardPopup()
{
	selector->init();
	showCardMenu( false );
}

void MainWindow::mousePressEvent(QMouseEvent *event)
{
	for(auto warning: warnings)
	{
		if(warning->underMouse())
		{
			closeWarning(warning);
			break;
		}
	}

	if(ribbon && ribbon->underMouse())
	{
		ribbon->flip();
		updateRibbon(ui->startScreen->currentIndex(), ribbon->isExpanded());
	}

	QWidget::mousePressEvent(event);
}

void MainWindow::mouseReleaseEvent(QMouseEvent *event)
{
	if(ui->logo->underMouse())
	{
		resetCryptoDoc();
		resetDigiDoc();
		navigateToPage(Pages::SignIntro);
	}
	else
	{
		QWidget::mousePressEvent(event);
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
			QString defaultDir = Settings(qApp->applicationName()).value("Client/DefaultDir").toString();
			QString filename = FileUtil::createNewFileName(files[0], QString(".%1").arg(ext), tr("signature container"), defaultDir);
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
			resetDigiDoc(signatureContainer.release());
			ui->signContainerPage->transition(digiDoc);
		}
	}
	else if(page == CryptoDetails)
	{
		navigate = false;
		std::unique_ptr<CryptoDoc> cryptoContainer(new CryptoDoc(this));

		if(create)
		{
			QString defaultDir = Settings(qApp->applicationName()).value("Client/DefaultDir").toString();
			QString filename = FileUtil::createNewFileName(files[0], ".cdoc", tr("crypto container"), defaultDir);
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
			resetCryptoDoc(cryptoContainer.release());
			ui->cryptoContainerPage->transition(cryptoDoc);
		}
	}

	if(navigate)
	{
		selectPageIcon( page < CryptoIntro ? ui->signature : (page == MyEid ? ui->myEid : ui->crypto));
		ui->startScreen->setCurrentIndex(page);
		updateWarnings();
	}
}

void MainWindow::onSignAction(int action, const QString &info1, const QString &info2)
{
	switch(action)
	{
	case SignatureAdd:
		sign();
		break;
	case SignatureMobile:
		signMobile(info1, info2);
		break;
	case SignatureWarning:
		showWarning(WarningText(info1, info2, SignDetails));
		ui->signature->warningIcon(true);
		break;
	case ClearSignatureWarning:
		ui->signature->warningIcon(false);
		closeWarnings(SignDetails);
		break;
	case ContainerCancel:
		resetDigiDoc();
		navigateToPage(Pages::SignIntro);
		break;
	case ContainerConvert:
		if(digiDoc)
			convertToCDoc();
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
	default:
		break;
	}
}

void MainWindow::convertToBDoc()
{
	QString ext = Settings(qApp->applicationName()).value("Client/Type", "bdoc").toString();
	QString filename = FileUtil::create(QFileInfo(cryptoDoc->fileName()), QString(".%1").arg(ext), tr("signature container"));
	if(filename.isNull())
		return;

	std::unique_ptr<DigiDoc> signatureContainer(new DigiDoc(this));
	signatureContainer->create(filename);

	// If encrypted, add whole cryptocontainer to signature container; otherwise content only
	if(cryptoDoc->state() == EncryptedContainer)
		signatureContainer->documentModel()->addFile(cryptoDoc->fileName());
	else
		signatureContainer->documentModel()->addTempFiles(cryptoDoc->documentModel()->tempFiles());

	resetCryptoDoc();
	resetDigiDoc(signatureContainer.release());

	ui->signContainerPage->transition(digiDoc);
	selectPageIcon(ui->signature);
	ui->startScreen->setCurrentIndex(SignDetails);

	FadeInNotification* notification = new FadeInNotification( this, WHITE, MANTIS, 110 );
	notification->start( tr("Converted to signed document!"), 750, 3000, 1200 );
}

void MainWindow::convertToCDoc()
{
	QString filename = FileUtil::create(QFileInfo(digiDoc->fileName()), ".cdoc", tr("crypto container"));
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

	resetCryptoDoc(cryptoContainer.release());
	resetDigiDoc(nullptr, false);
	ui->cryptoContainerPage->transition(cryptoDoc);
	selectPageIcon(ui->crypto);
	ui->startScreen->setCurrentIndex(CryptoDetails);

	FadeInNotification* notification = new FadeInNotification( this, WHITE, MANTIS, 110 );
	notification->start( tr("Converted to crypto container!"), 750, 3000, 1200 );
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
		resetCryptoDoc();
		navigateToPage(Pages::CryptoIntro);
		break;
	case ContainerConvert:
		if(cryptoDoc)
			convertToBDoc();
		break;
	case DecryptContainer:
		if(decrypt())
		{
			ui->cryptoContainerPage->transition(cryptoDoc);

			FadeInNotification* notification = new FadeInNotification( this, WHITE, MANTIS, 110 );
			notification->start( tr("Decryption succeeded"), 750, 3000, 1200 );
		}
		else if((qApp->signer()->tokensign().flags() & TokenData::PinLocked))
		{
			smartcard->reload(); // smartcard should also know that PIN is blocked.
			showPinBlockedWarning(smartcard->data());
		}
		break;
	case EncryptContainer:
		if(encrypt())
		{
			ui->cryptoContainerPage->transition(cryptoDoc);

			FadeInNotification* notification = new FadeInNotification( this, WHITE, MANTIS, 110 );
			notification->start( tr("Encryption succeeded"), 750, 3000, 1200 );
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

void MainWindow::openFiles(const QStringList &files)
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
		- ask if new container should be created with signature container and
		  files to be opened;
	2.4 else if EncryptedContainer || DecryptedContainer
			- ask if should be closed and handle open
*/
	auto current = ui->startScreen->currentIndex();
	QStringList content(files);
	ContainerState state = currentState();
	Pages page = SignDetails;
	bool create = true;
	switch(state)
	{
	case Uninitialized:
	// Case 1.
		if(content.size() == 1)
		{
			auto fileType = FileUtil::detect(content[0]);
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
		page = (state == ContainerState::UnencryptedContainer) ? CryptoDetails : SignDetails;
		create = false;
		if(validateFiles(page == CryptoDetails ? cryptoDoc->fileName() : digiDoc->fileName(), content))
		{
			DocumentModel* model = (current == CryptoDetails) ?
				cryptoDoc->documentModel() : digiDoc->documentModel();
			for(auto file: content)
				model->addFile(file);
		}
		break;
	case ContainerState::EncryptedContainer:
	case ContainerState::DecryptedContainer:
		// TODO: new container???
		create = false;
		break;
	default:
		if(wrapContainer())
			content.insert(content.begin(), digiDoc->fileName());
		else
			create = false;

		page = SignDetails;
		break;
	}

	if(create || current != page)
	{
		WaitDialog waitDialog(this);
		waitDialog.setText(tr("Opening"));
		waitDialog.open();
		navigateToPage(page, content, create);
	}
}

void MainWindow::open(const QStringList &params, bool crypto)
{
	if (crypto)
		navigateToPage(Pages::CryptoIntro);

	QStringList files;
	for(auto param: params)
	{
		const QFileInfo f(param);
		if(!f.isFile())
			continue;
		files << param;
	}

	if(!files.isEmpty())
		openFiles(files);
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

void MainWindow::resetCryptoDoc(CryptoDoc *doc)
{
	ui->cryptoContainerPage->clear();
	delete cryptoDoc;
	cryptoDoc = doc;
}

void MainWindow::resetDigiDoc(DigiDoc *doc, bool warnOnChange)
{
	if(warnOnChange && digiDoc && digiDoc->isModified())
	{
		QString warning, cancelTxt, saveTxt;
		if(digiDoc->state() == UnsignedContainer)
		{
			warning = tr("You've added file(s) to container, but these are not signed yet. Keep the unsigned container or remove it?");
			cancelTxt = tr("REMOVE");
			saveTxt = tr("KEEP");
		}
		else
		{
			warning = tr("You've changed the open container but have not saved any changes. Save the changes or close without saving?");
			cancelTxt = tr("DO NOT SAVE");
			saveTxt = tr("SAVE");
		}

		WarningDialog dlg(warning, this);
		dlg.setCancelText(cancelTxt);
		dlg.addButton(saveTxt, ContainerSave);
		dlg.exec();
		if(dlg.result() == ContainerSave)
			save();
	}

	ui->signature->warningIcon(false);

	ui->signContainerPage->clear();
	delete digiDoc;
	closeWarnings(SignDetails);
	digiDoc = doc;
	if(digiDoc)
		connect(digiDoc, &DigiDoc::operation, this, &MainWindow::operation);
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
		connect( cardPopup.get(), &CardPopup::activated, qApp->signer(), &QSigner::selectSignCard, Qt::QueuedConnection );
		connect( cardPopup.get(), &CardPopup::activated, qApp->signer(), &QSigner::selectAuthCard, Qt::QueuedConnection );
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

	closeWarnings(-1);

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
		ui->myEid->invalidIcon( !t.authCert().isValid() || !t.signCert().isValid() );
		updateCardWarnings();
		showIdCardAlerts( t );
		showPinBlockedWarning( t );
	}
	else
	{
		emit ui->signContainerPage->cardChanged();
		emit ui->cryptoContainerPage->cardChanged();

		if ( !QPCSC::instance().serviceRunning() )
			noReader_NoCard_Loading_Event(NoCardInfo::NoPCSC);
		else if ( t.readers().isEmpty() )
			noReader_NoCard_Loading_Event(NoCardInfo::NoReader);
		else if ( t.card().isEmpty() )
			noReader_NoCard_Loading_Event(NoCardInfo::NoCard);
		else
			noReader_NoCard_Loading_Event(NoCardInfo::Loading);
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

void MainWindow::showSettings(int page)
{
	QSmartCardData t = smartcard->data();
	QString appletVersion = t.isNull() ? QString() : t.appletVersion();
	SettingsDialog dlg(page, this, appletVersion);

	connect(&dlg, &SettingsDialog::langChanged, this,
			[this](const QString& lang )
			{
				qApp->loadTranslation( lang );
				ui->retranslateUi(this);
			}
	);
	connect(&dlg, &SettingsDialog::removeOldCert, this,	&MainWindow::removeOldCert);
	dlg.exec();
}

bool MainWindow::sign()
{
	if(!checkConnection())
		return false;

	AccessCert access(this);
	if( !access.validate() )
		return false;
	WaitDialog waitDialog(this);
	waitDialog.setText(tr("Signing"));
	waitDialog.open();

	QString role = Settings(qApp->applicationName()).value( "Client/Role" ).toString();
	QString city = Settings(qApp->applicationName()).value( "Client/City" ).toString();
	QString state = Settings(qApp->applicationName()).value( "Client/State" ).toString();
	QString country = Settings(qApp->applicationName()).value( "Client/Country" ).toString();
	QString zip = Settings(qApp->applicationName()).value( "Client/Zip" ).toString();
	if(digiDoc->sign(city, state, zip, country, role, ""))
	{
		access.increment();
		if(save())
		{
			ui->signContainerPage->transition(digiDoc);
			waitDialog.close();

			FadeInNotification* notification = new FadeInNotification(this, WHITE, MANTIS, 110);
			notification->start(tr("The container has been successfully signed!"), 750, 3000, 1200);
			return true;
		}
	}
	else if((qApp->signer()->tokensign().flags() & TokenData::PinLocked))
	{
		smartcard->reload();
		showPinBlockedWarning(smartcard->data());
	}

	return false;
}

bool MainWindow::signMobile(const QString &idCode, const QString &phoneNumber)
{
	if(!checkConnection())
		return false;

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
	if(save())
	{
		ui->signContainerPage->transition(digiDoc);

		FadeInNotification* notification = new FadeInNotification(this, WHITE, MANTIS, 110);
		notification->start(tr("The container has been successfully signed!"), 750, 3000, 1200);
	}

	return true;
}

void MainWindow::updateCardData()
{
	if( cardPopup )
		cardPopup->update( smartcard );
}

void MainWindow::noReader_NoCard_Loading_Event(NoCardInfo::Status status)
{
	ui->idSelector->hide();
	if(status == NoCardInfo::Loading)
		Application::setOverrideCursor(Qt::BusyCursor);

	ui->noCardInfo->show();
	ui->noCardInfo->update(status);
	ui->infoStack->clearData();
	ui->cardInfo->clearPicture();
	ui->infoStack->clearPicture();
	ui->infoStack->hide();
	ui->accordion->hide();
	ui->accordion->clearOtherEID();
	ui->myEid->invalidIcon( false );
	ui->myEid->warningIcon( false );
	clearWarning(UPDATE_CERT_WARNING);
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
			showWarning(WarningText(XmlReader::emailErr(error.toUInt())));
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

void MainWindow::removeCryptoFile(int index)
{
	if(!cryptoDoc)
		return;

	if(removeFile(cryptoDoc->documentModel(), index))
	{
		if(QFile::exists(cryptoDoc->fileName()))
			QFile::remove(cryptoDoc->fileName());
		resetCryptoDoc();
		navigateToPage(Pages::CryptoIntro);
	}
}

bool MainWindow::removeFile(DocumentModel *model, int index)
{
	bool rc = false;
	auto count = model->rowCount();
	if(count != 1)
	{
		model->removeRows(index, 1);
	}
	else
	{
		WarningDialog dlg(tr("You are about to delete the last file in the container, it is removed along with the container."), this);
		dlg.setCancelText(tr("CANCEL"));
		dlg.addButton(tr("REMOVE"), ContainerSave);
		dlg.exec();
		rc = (dlg.result() == ContainerSave);
	}

	return rc;
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

void MainWindow::removeSignatureFile(int index)
{
	if(!digiDoc)
		return;

	if(removeFile(digiDoc->documentModel(), index))
	{
		if(QFile::exists(digiDoc->fileName()))
			QFile::remove(digiDoc->fileName());
		resetDigiDoc(nullptr, false);
		navigateToPage(Pages::SignIntro);
	}
}

void MainWindow::savePhoto( const QPixmap *photo )
{
	QString fileName = QFileDialog::getSaveFileName(this,
		tr("Save photo"), "",
		tr("Photo (*.jpg);;All Files (*)"));

	photo->save( fileName, "JPEG", 100 );
}

void MainWindow::showUpdateCertWarning()
{
	for(auto warning: warnings)
	{
		if(warning->property(UPDATE_CERT_WARNING).toBool())
			return;
	}

	emit ui->accordion->showCertWarnings();
	showWarning(WarningText(tr("Card certificates need updating. Updating takes 2-10 minutes and requires a live internet connection. The card must not be removed from the reader before the end of the update."),
		QString("<a href='#update-Certificate'><span style='color:rgb(53, 55, 57)'>%1</span></a>").arg(tr("Update")),
		false, UPDATE_CERT_WARNING));
}

void MainWindow::showWarning(const WarningText &warningText)
{
	if(!warningText.property.isEmpty())
	{
		for(auto warning: warnings)
		{
			if(warning->property(warningText.property.toLatin1()).toBool())
				return;
		}
	}
	WarningItem *warning = new WarningItem(warningText, ui->page);
	auto layout = qobject_cast<QBoxLayout*>(ui->page->layout());
	warnings << warning;
	connect(warning, &WarningItem::linkActivated, this, &MainWindow::warningClicked);
	layout->insertWidget(warnings.size(), warning);

	updateWarnings();
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


void MainWindow::updateRibbon(int page, bool expanded)
{
	short count = 0;
	for(auto warning: warnings)
	{
		if(warning->appearsOnPage(page))
		{
			warning->setVisible(expanded || !count);
			count++;
		}
	}
	if (count > 1)
	{
		QSize	sizeBeforeAdjust = size();

		adjustSize();

		QSize	sizeAfterAdjust = size();
		// keep the size set-up by user, if possible
		resize(sizeBeforeAdjust.width(), sizeBeforeAdjust.height() > sizeAfterAdjust.height() ? sizeBeforeAdjust.height() : sizeAfterAdjust.height());
	}
}

void MainWindow::updateWarnings()
{
	int page = ui->startScreen->currentIndex();
	int count = 0;
	bool expanded = true;
	for(auto warning: warnings)
	{
		if(warning->appearsOnPage(page))
			count++;
		else
			warning->hide();
	}

	if(!count)
		ui->topBarShadow->setStyleSheet("background: qlineargradient(x1: 0, y1: 0, x2: 0, y2: 1, stop: 0 #c8c8c8, stop: 1 #F4F5F6); \nborder: none;");
	else
		ui->topBarShadow->setStyleSheet("background: qlineargradient(x1: 0, y1: 0, x2: 0, y2: 1, stop: 0 #b5aa92, stop: 1 #F8DDA7); \nborder: none;");

	if(count < 2)
	{
		if(ribbon)
		{
			ribbon->close();
			delete ribbon;
			ribbon = nullptr;
			for(auto warning: warnings)
			{
				if(warning->appearsOnPage(page))
					warning->show();
			}
		}
	}
	else if(ribbon)
	{
		ribbon->setCount(count - 1);
		expanded = ribbon->isExpanded();
	}
	else
	{
		ribbon = new WarningRibbon(count - 1, ui->page);
		auto layout = qobject_cast<QBoxLayout*>(ui->page->layout());
		layout->insertWidget(warnings.size() + 1, ribbon);
		ribbon->show();
		expanded = ribbon->isExpanded();
	}

	updateRibbon(page, expanded);
}

bool MainWindow::validateFiles(const QString &container, const QStringList &files)
{
	// Check that container is not dropped into itself
	QFileInfo containerInfo(container);
	for(auto file: files)
	{
		if(containerInfo == QFileInfo(file))
		{
			WarningDialog dlg(tr("Cannot add container to same container\n%1").arg(container), this);
			dlg.setCancelText(tr("CANCEL"));
			dlg.exec();	
			return false;
		}
	}

	return true;
}

void MainWindow::warningClicked(const QString &link)
{
	if(link == "#update-Certificate")
		updateCertificate();
	else if(link.startsWith("#invalid-signature-"))
		emit ui->signContainerPage->details(link.right(link.length()-19));
	else if(link == "#unblock-PIN1")
		ui->accordion->changePin1Clicked (false, true);
	else if(link == "#unblock-PIN2")
		ui->accordion->changePin2Clicked (false, true);
}

bool MainWindow::wrapContainer()
{
	WarningDialog dlg(tr("Files can not be added to the signed container. The system will create a new container which shall contain the signed document and the files you wish to add."), this);
	dlg.setCancelText(tr("CANCEL"));
	dlg.addButton(tr("CONTINUE"), ContainerSave);
	dlg.exec();
	if(dlg.result() == ContainerSave)
		return true;

	return false;
}

void MainWindow::showIdCardAlerts(const QSmartCardData& t)
{
	if(smartcard->property( "lastcard" ).toString() != t.card() &&
		t.version() == QSmartCardData::VER_3_4 &&
		(!t.authCert().validateEncoding() || !t.signCert().validateEncoding()))
	{
		qApp->showWarning( tr("Your ID-card certificates cannot be renewed starting from 01.07.2017. Your document is still valid until its expiring date and it can be used to login to e-services and give digital signatures. If there are problems using Your ID-card in e-services please contact ID-card helpdesk by phone (+372) 677 3377 or visit Police and Border Guard Board service point.&lt;br /&gt;&lt;br /&gt;&lt;a href=&quot;http://id.ee/?id=30519&amp;read=38011&quot;&gt;More info&lt;/a&gt;"));
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

void MainWindow::showPinBlockedWarning(const QSmartCardData& t)
{
	if(	t.retryCount( QSmartCardData::Pin2Type ) == 0 )
	{
		showWarning(WarningText(
			VerifyCert::tr("PIN%1 has been blocked because PIN%1 code has been entered incorrectly 3 times. Unblock to reuse PIN%1.").arg("2"),
			QString("<a href='#unblock-PIN2'><span style='color:rgb(53, 55, 57)'>%1</span></a>").arg(VerifyCert::tr("UNBLOCK")),
			-1,
			UNBLOCK_PIN2_WARNING));
		emit ui->signContainerPage->cardChanged(); // hide Sign button
	}
	if(	t.retryCount( QSmartCardData::Pin1Type ) == 0 )
	{
		showWarning(WarningText(
			VerifyCert::tr("PIN%1 has been blocked because PIN%1 code has been entered incorrectly 3 times. Unblock to reuse PIN%1.").arg("1"),
			QString("<a href='#unblock-PIN1'><span style='color:rgb(53, 55, 57)'>%1</span></a>").arg(VerifyCert::tr("UNBLOCK")),
			-1,
			UNBLOCK_PIN1_WARNING));
		emit ui->cryptoContainerPage->cardChanged(); // hide Decrypt button
	}
}

void MainWindow::updateKeys(QList<CKey> keys)
{
	if(!cryptoDoc)
		return;

	for(int i = cryptoDoc->keys().size() - 1; i >= 0; i--)
		cryptoDoc->removeKey(i);
	for(auto key: keys)
		cryptoDoc->addKey(key);
	ui->cryptoContainerPage->update(cryptoDoc);
}
