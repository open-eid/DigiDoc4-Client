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
#include "PrintSheet.h"
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
#include "dialogs/MobileProgress.h"
#include "dialogs/RoleAddressDialog.h"
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
#include <QLoggingCategory>
#include <QMessageBox>
#include <QMimeData>
#include <QSvgWidget>
#include <QtCore/QUrlQuery>
#include <QtGui/QDesktopServices>
#include <QtPrintSupport/QPrinter>
#include <QtPrintSupport/QPrinterInfo>
#include <QtPrintSupport/QPrintPreviewDialog>

using namespace ria::qdigidoc4;
using namespace ria::qdigidoc4::colors;

Q_LOGGING_CATEGORY(MLog, "qdigidoc4.MainWindow")

MainWindow::MainWindow( QWidget *parent ) :
	QWidget( parent ),
	ui( new Ui::MainWindow )
{
	setAttribute(Qt::WA_DeleteOnClose, true);

	QFont condensed11 = Styles::font( Styles::Condensed, 11 );
	QFont condensed14 = Styles::font( Styles::Condensed, 14 );
	QFont regular20 = Styles::font( Styles::Regular, 20 );

	Settings s(qApp->applicationName());
	if(s.value(QStringLiteral("Client/Type")) == "ddoc")
		s.remove(QStringLiteral("Client/Type"));

	ui->setupUi(this);

	ui->version->setFont( Styles::font( Styles::Regular, 12 ) );
	ui->version->setText(QStringLiteral("%1<a href='#show-diagnostics'><span style='color:#006EB5;'>%2</span></a>")
		.arg(tr("Ver. "), qApp->applicationVersion()));
	connect(ui->version, &QLabel::linkActivated, this, 
		[this] {showSettings(SettingsDialog::DiagnosticsSettings);});

	QSvgWidget* coatOfArms = new QSvgWidget(ui->logo);
	coatOfArms->setStyleSheet(QStringLiteral("border: none;"));
	coatOfArms->load(QStringLiteral(":/images/Logo_small.svg"));
	coatOfArms->resize( 80, 32 );
	coatOfArms->move( 15, 17 );
	ui->signature->init( Pages::SignIntro, ui->signatureShadow, true );
	ui->crypto->init( Pages::CryptoIntro, ui->cryptoShadow, false );
	ui->myEid->init( Pages::MyEid, ui->myEidShadow, false );

	connect(ui->signature, &PageIcon::activated, this, &MainWindow::clearPopups);
	connect(ui->signature, &PageIcon::activated, this, &MainWindow::pageSelected);
	connect(ui->crypto, &PageIcon::activated, this, &MainWindow::clearPopups);
	connect(ui->crypto, &PageIcon::activated, this, &MainWindow::pageSelected);
	connect(ui->myEid, &PageIcon::activated, this, &MainWindow::clearPopups);
	connect(ui->myEid, &PageIcon::activated, this, &MainWindow::pageSelected);

	selector = new DropdownButton(QStringLiteral(":/images/arrow_down.svg"), QStringLiteral(":/images/arrow_down_selected.svg"), ui->selector);
	selector->hide();
	selector->resize( 12, 6 );
	selector->move( 9, 32 );
	selector->setCursor( Qt::PointingHandCursor );
	ui->help->installEventFilter(new ButtonHoverFilter(QStringLiteral(":/images/icon_Abi.svg"), QStringLiteral(":/images/icon_Abi_hover.svg"), this));
	ui->settings->installEventFilter(new ButtonHoverFilter(QStringLiteral(":/images/icon_Seaded.svg"), QStringLiteral(":/images/icon_Seaded_hover.svg"), this));
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

	// Refresh ID card info in card widget
	connect(qApp->signer(), &QSigner::dataChanged, this, &MainWindow::showCardStatus);
	connect(qApp->signer(), &QSigner::updateRequired, this, &MainWindow::showUpdateCertWarning);
	// Refresh card info on "My EID" page
	connect(qApp->smartcard(), &QSmartCard::dataChanged, this, &MainWindow::updateMyEid);
	// Show card pop-up menu
	connect( selector, &DropdownButton::dropdown, this, &MainWindow::showCardMenu );

	connect(this, &MainWindow::clearPopups, ui->signContainerPage, &ContainerPage::clearPopups);
	connect(this, &MainWindow::clearPopups, this, &MainWindow::hideCardPopup);

	ui->accordion->init();

	connect(ui->signIntroButton, &QPushButton::clicked, this, &MainWindow::openContainer);
	connect(ui->cryptoIntroButton, &QPushButton::clicked, this, &MainWindow::openContainer);
	connect(buttonGroup, static_cast<void(QButtonGroup::*)(int)>(&QButtonGroup::buttonClicked), this, &MainWindow::buttonClicked);
	connect(ui->signContainerPage, &ContainerPage::action, this, &MainWindow::onSignAction);
	connect(ui->signContainerPage, &ContainerPage::addFiles, this, [this](const QStringList &files) { openFiles(files, true); } );
	connect(ui->signContainerPage, &ContainerPage::fileRemoved, this, &MainWindow::removeSignatureFile);
	connect(ui->signContainerPage, &ContainerPage::removed, this, &MainWindow::removeSignature);
	connect(ui->signContainerPage, &ContainerPage::warning, this, [this](const WarningText &warningText) {
		showWarning(warningText);
		ui->signature->warningIcon(true);
	});

	connect(ui->cryptoContainerPage, &ContainerPage::action, this, &MainWindow::onCryptoAction);
	connect(ui->cryptoContainerPage, &ContainerPage::addFiles, this, [this](const QStringList &files) { openFiles(files, true); } );
	connect(ui->cryptoContainerPage, &ContainerPage::fileRemoved, this, &MainWindow::removeCryptoFile);
	connect(ui->cryptoContainerPage, &ContainerPage::keysSelected, this, &MainWindow::updateKeys);
	connect(ui->cryptoContainerPage, &ContainerPage::removed, this, &MainWindow::removeAddress);

	connect(ui->accordion, &Accordion::checkEMail, this, &MainWindow::getEmailStatus);   // To check e-mail
	connect(ui->accordion, &Accordion::activateEMail, this, &MainWindow::activateEmail);   // To activate e-mail
	connect(ui->infoStack, &InfoStack::photoClicked, this, &MainWindow::photoClicked);
	connect(ui->cardInfo, &CardWidget::photoClicked, this, &MainWindow::photoClicked);   // To load photo
	connect(ui->cardInfo, &CardWidget::selected, this, [this]() { if( selector ) selector->press(); });

	showCardStatus();
	updateMyEid();
	connect(ui->accordion, &Accordion::changePin1Clicked, this, &MainWindow::changePin1Clicked);
	connect(ui->accordion, &Accordion::changePin2Clicked, this, &MainWindow::changePin2Clicked);
	connect(ui->accordion, &Accordion::changePukClicked, this, &MainWindow::changePukClicked);
	connect(ui->accordion, &Accordion::certDetailsClicked, this, &MainWindow::certDetailsClicked);
}

MainWindow::~MainWindow()
{
	delete ui;
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
			selectPage(SignDetails);
		}
	}
	else if(toPage == CryptoIntro)
	{
		if(cryptoDoc)
		{
			navigate = false;
			selectPage(CryptoDetails);
		}
	}

	if(navigate)
		navigateToPage(toPage);
}

void MainWindow::adjustDrops()
{
	if(currentState() == SignedContainer)
		setAcceptDrops(false);
	else
		setAcceptDrops(true);
}

void MainWindow::browseOnDisk( const QString &fileName )
{
	if(!QFileInfo::exists(fileName))
		return;
	QUrl url = QUrl::fromLocalFile( fileName );
	url.setScheme(QStringLiteral("browse"));
	QDesktopServices::openUrl( url );
}

void MainWindow::buttonClicked( int button )
{
	switch( button )
	{
	case HeadHelp:
	{
		QDesktopServices::openUrl( QUrl( Common::helpUrl() ) );
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

QSet<QString> MainWindow::cards() const
{
	return qApp->signer()->tokenauth().cards().toSet()
		.unite(qApp->signer()->tokensign().cards().toSet());
}

void MainWindow::changeEvent(QEvent* event)
{
	if (event->type() == QEvent::LanguageChange)
	{
		ui->retranslateUi(this);

		ui->version->setText(QStringLiteral("%1<a href='#show-diagnostics'><span style='color:#006EB5;'>%2</span></a>")
			.arg(tr("Ver. "), qApp->applicationVersion()));
	}

	QWidget::changeEvent(event);
}

void MainWindow::clearOverlay()
{
	overlay->close();
	overlay.reset( nullptr );
}

void MainWindow::clearWarning(const QList<int> &warningTypes)
{
	for(auto warning: warnings)
	{
		if(warningTypes.contains(warning->warningType()))
			closeWarning(warning, true);
	}
	updateWarnings();
}

void MainWindow::closeEvent(QCloseEvent * /*event*/)
{
	resetCryptoDoc();
	resetDigiDoc();
	ui->startScreen->setCurrentIndex(SignIntro);
}

bool MainWindow::closeWarning(WarningItem *warning, bool force)
{
	if(force || warning->warningType() != WarningType::UpdateCertWarning)
	{
		warnings.removeOne(warning);
		warning->close();
		warning->deleteLater();
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

QString MainWindow::cryptoPath()
{
	return cryptoDoc ? cryptoDoc->fileName() : QString();
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

	WaitDialogHolder waitDialog(this);

	return cryptoDoc->decrypt();
}

QString MainWindow::digiDocPath()
{
	return digiDoc ? digiDoc->fileName() : QString();
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
		for(const auto &url: mimeData->urls())
		{
			if(url.scheme() == QStringLiteral("file"))
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

	if(!files.isEmpty())
		openFiles(files, true);
}

bool MainWindow::encrypt()
{
	if(!cryptoDoc)
		return false;

	WaitDialogHolder waitDialog(this, tr("Encrypting"));

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
	emit clearPopups();
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
			QString ext = Settings(qApp->applicationName()).value(QStringLiteral("Client/Type"), "bdoc").toString();
			QString defaultDir = Settings().value(QStringLiteral("Client/DefaultDir")).toString();
			QString filename = FileUtil::createNewFileName(files[0], QStringLiteral(".%1").arg(ext), tr("signature container"), defaultDir);
			if(!filename.isNull())
			{
				signatureContainer->create(filename);
				for(const auto &file: files)
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
			QString defaultDir = Settings().value(QStringLiteral("Client/DefaultDir")).toString();
			QString filename = FileUtil::createNewFileName(files[0], QStringLiteral(".cdoc"), tr("crypto container"), defaultDir);
			if(!filename.isNull())
			{
				cryptoContainer->clear(filename);
				for(const auto &file: files)
					cryptoContainer->documentModel()->addFile(file);
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
			ui->cryptoContainerPage->transition(cryptoDoc, cryptoDoc->canDecrypt(qApp->signer()->tokenauth().cert()));
		}
	}

	if(navigate)
		selectPage(page);
}

void MainWindow::onSignAction(int action, const QString &info1, const QString &info2)
{
	switch(action)
	{
	case SignatureAdd:
	case SignatureToken:
		if(digiDoc->isService())
			wrapAndSign();
		else
			sign();
		break;
	case SignatureMobile:
		if(digiDoc->isService())
			wrapAndMobileSign(info1, info2);
		else
			signMobile(info1, info2);
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
	case ContainerSaveAs:
		save(true);
		break;
	case ContainerEmail:
		if(digiDoc)
			containerToEmail(digiDoc->fileName());
		break;
	case ContainerSummary:
		if(digiDoc)
			containerSummary();
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
	if(!wrap(cryptoDoc->fileName(), cryptoDoc->state() == EncryptedContainer))
		return;

	FadeInNotification* notification = new FadeInNotification( this, WHITE, MANTIS, 110 );
	notification->start( tr("Converted to signed document!"), 750, 3000, 1200 );
}

void MainWindow::convertToCDoc()
{
	QString filename = FileUtil::create(QFileInfo(digiDoc->fileName()), QStringLiteral(".cdoc"), tr("crypto container"));
	if(filename.isNull())
		return;

	std::unique_ptr<CryptoDoc> cryptoContainer(new CryptoDoc(this));
	cryptoContainer->clear(filename);

	// If signed, add whole signed document to cryptocontainer; otherwise content only
	if(digiDoc->state() == SignedContainer)
		cryptoContainer->documentModel()->addFile(digiDoc->fileName());
	else
		cryptoContainer->documentModel()->addTempFiles(digiDoc->documentModel()->tempFiles());

	auto cardData = qApp->signer()->tokenauth();
	if(!cardData.cert().isNull())
		cryptoContainer->addKey(CKey(cardData.cert()));

	resetCryptoDoc(cryptoContainer.release());
	resetDigiDoc(nullptr, false);
	ui->cryptoContainerPage->transition(cryptoDoc, false);
	selectPage(CryptoDetails);

	FadeInNotification* notification = new FadeInNotification( this, WHITE, MANTIS, 110 );
	notification->start( tr("Converted to crypto container!"), 750, 3000, 1200 );
}

void MainWindow::moveCryptoContainer()
{
	QString to = selectFile(tr("Move file"),cryptoDoc->fileName(), true);
	if(!to.isNull() && cryptoDoc->move(to))
		ui->cryptoContainerPage->moved(to);
}

void MainWindow::moveSignatureContainer()
{
	QString to = selectFile(tr("Move file"),digiDoc->fileName(), true);
	if(!to.isNull() && digiDoc->move(to))
		ui->signContainerPage->moved(to);
}

void MainWindow::onCryptoAction(int action, const QString &/*id*/, const QString &/*phone*/)
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
			ui->cryptoContainerPage->transition(cryptoDoc, false);

			FadeInNotification* notification = new FadeInNotification( this, WHITE, MANTIS, 110 );
			notification->start( tr("Decryption succeeded"), 750, 3000, 1200 );
		}
		else if((qApp->signer()->tokenauth().flags() & TokenData::PinLocked))
		{
			qApp->smartcard()->reload(); // QSmartCard should also know that PIN1 is blocked.
			showPinBlockedWarning(qApp->smartcard()->data());
		}
		break;
	case EncryptContainer:
		if(encrypt())
		{
			ui->cryptoContainerPage->transition(cryptoDoc, cryptoDoc->canDecrypt(qApp->signer()->tokenauth().cert()));

			FadeInNotification* notification = new FadeInNotification( this, WHITE, MANTIS, 110 );
			notification->start( tr("Encryption succeeded"), 750, 3000, 1200 );
		}
		break;
	case ContainerSaveAs:
	{
		if(!cryptoDoc)
			break;
		QString target = FileUtil::createNewFileName(cryptoDoc->fileName(), QStringLiteral(".cdoc"), tr("crypto container"), QString());
		if( !FileDialog::fileIsWritable(target) &&
			QMessageBox::Yes == QMessageBox::warning(this, tr("DigiDoc4 client"),
				tr("Cannot alter container %1. Save different location?").arg(target),
				QMessageBox::Yes|QMessageBox::No, QMessageBox::Yes))
		{
			QString file = selectFile(tr("Save file"), target, true);
			if(!file.isEmpty())
				cryptoDoc->saveCopy(file);
		}
		cryptoDoc->saveCopy(target);
		break;
	}
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

void MainWindow::openFile(const QString &file)
{
	QDesktopServices::openUrl(QUrl::fromLocalFile(file));
}

void MainWindow::openFiles(const QStringList &files, bool addFile, bool forceCreate)
{
/*
	1. If containers are not open:
		- If on myEid page and either crypto- or signature document, open corresponding view
		- If on encrypt page, open encryption view
		- else open signing view

	2. If container open:
	2.1 if UnsignedContainer | UnsignedSavedContainer | UnencryptedContainer
		- If dropped/file add selected, add file to files to be signed/encrypted
		- else open file in another view
	2.3 else
		- If dropped/file add selected, ask if new container should be created with 
		  current container and files to be opened;
		- else open file in another view
*/
	auto current = ui->startScreen->currentIndex();
	QStringList content(files);
	ContainerState state = currentState();
	Pages page = (current == CryptoIntro) ? CryptoDetails : SignDetails;
	bool create = true;
	switch(state)
	{
	case Uninitialized:
	// Case 1.
		if(content.size() == 1)
		{
			auto fileType = FileUtil::detect(content[0]);
			if(current == MyEid)
				page = (fileType == CryptoDocument) ? CryptoDetails : SignDetails;

			if(!forceCreate && (
				(fileType == CryptoDocument && page == CryptoDetails) ||
				(fileType == SignatureDocument && page == SignDetails)))
			{
				create = false;
			}
		}
		break;
	case ContainerState::UnsignedContainer:
	case ContainerState::UnsignedSavedContainer:
	case ContainerState::UnencryptedContainer:
		if(addFile)
		{
			page = (state == ContainerState::UnencryptedContainer) ? CryptoDetails : SignDetails;
			create = false;
			if(validateFiles(page == CryptoDetails ? cryptoDoc->fileName() : digiDoc->fileName(), content))
			{
				DocumentModel* model = (current == CryptoDetails) ?
					cryptoDoc->documentModel() : digiDoc->documentModel();
				for(const auto &file: content)
					model->addFile(file);
				selectPage(page);
				return;
			}
		}
		else
		{
			// If browsed (double-clicked in Explorer/Finder, clicked on bdoc/cdoc in opened container)
			// or recently opened file is opened, then new container should be created.
			create = true;
			page = (state == ContainerState::UnencryptedContainer) ? SignDetails : CryptoDetails;
		}
		break;
	default:
		if(addFile)
		{
			bool crypto = state & CryptoContainers;
			if(wrapContainer(!crypto))
				content.insert(content.begin(), crypto ? cryptoDoc->fileName() : digiDoc->fileName());
			else
				create = false;

			page = crypto ? CryptoDetails : SignDetails;
		}
		else
		{
			// If browsed (double-clicked in Explorer/Finder, clicked on bdoc/cdoc in opened container)
			// or recently opened file is opened, then new container should be created.
			create = true;
			page = (state & CryptoContainers) ? SignDetails : CryptoDetails;
		}
		break;
	}

	if(create || current != page)
	{
		WaitDialogHolder waitDialog(this, tr("Opening"));
		navigateToPage(page, content, create);
	}
}

void MainWindow::open(const QStringList &params, bool crypto, bool sign)
{
	if (crypto && !sign)
		navigateToPage(Pages::CryptoIntro);
	else
		navigateToPage(Pages::SignIntro);

	QStringList files;
	for(const auto &param: params)
	{
		if(QFileInfo(param).isFile())
			files << param;
	}

	if(!files.isEmpty())
		openFiles(files, false, sign);
}

void MainWindow::openContainer()
{
	auto current = ui->startScreen->currentIndex();
	auto filter = QString();

	if(current == CryptoIntro)
		filter = QFileDialog::tr("All Files (*)") + ";;" + tr("Documents (%1)").arg(QStringLiteral("*.cdoc"));
	else if(current == SignIntro)
		filter = QFileDialog::tr("All Files (*)") + ";;" + tr("Documents (%1%2)").arg(
				QStringLiteral("*.bdoc *.ddoc *.asice *.sce *.asics *.scs *.edoc *.adoc"),
				qApp->confValue(Application::SiVaUrl).toString().isEmpty() ? QString() : QStringLiteral(" *.pdf"));

	QStringList files = FileDialog::getOpenFileNames(this, tr("Select documents"), QString(), filter);
	if(!files.isEmpty())
		openFiles(files);
}

void MainWindow::operation(int op, bool started)
{
	qCDebug(MLog) << "Op " << op << (started ? " started" : " ended");
}

void MainWindow::resetCryptoDoc(CryptoDoc *doc)
{
	ui->cryptoContainerPage->clear();
	delete cryptoDoc;
	cryptoDoc = doc;
	if(cryptoDoc)
		connect(cryptoDoc->documentModel(), &DocumentModel::openFile, this, &MainWindow::openFile);
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
	{
		connect(digiDoc, &DigiDoc::operation, this, &MainWindow::operation);
		connect(digiDoc->documentModel(), &DocumentModel::openFile, this, &MainWindow::openFile);
	}
}

void MainWindow::resizeEvent(QResizeEvent * /*event*/)
{
	ui->version->move( ui->version->geometry().x(), ui->leftBar->height() - ui->version->height() - 11 );
}

bool MainWindow::save(bool saveAs)
{
	if(!digiDoc)
		return false;

	QString target = digiDoc->fileName().normalized(QString::NormalizationForm_C);
	if(saveAs)
		target = selectFile(tr("Save file"), target, true);

	if( !FileDialog::fileIsWritable(target) &&
		QMessageBox::Yes == QMessageBox::warning(this, tr("DigiDoc4 client"),
			tr("Cannot alter container %1. Save different location?").arg(target),
			QMessageBox::Yes|QMessageBox::No, QMessageBox::Yes))
	{
		QString file = selectFile(tr("Save file"), target, true);
		if(!file.isEmpty())
			return saveAs ? digiDoc->saveAs(file) : digiDoc->save(file);
	}
	return saveAs ? digiDoc->saveAs(target) : digiDoc->save(target);
}

QString MainWindow::selectFile( const QString &title, const QString &filename, bool fixedExt )
{
	static const QString adoc = tr("Documents (%1)").arg(QStringLiteral("*.adoc"));
	static const QString bdoc = tr("Documents (%1)").arg(QStringLiteral("*.bdoc"));
	static const QString cdoc = tr("Documents (%1)").arg(QStringLiteral("*.cdoc"));
	static const QString edoc = tr("Documents (%1)").arg(QStringLiteral("*.edoc"));
	static const QString asic = tr("Documents (%1)").arg(QStringLiteral("*.asice *.sce"));
	static const QString ext = QFileInfo( filename ).suffix().toLower();
	QStringList exts;
	QString active;
	if( fixedExt )
	{
		if(ext == QStringLiteral("bdoc")) exts << bdoc;
		if(ext == QStringLiteral("cdoc")) exts << cdoc;
		if(ext == QStringLiteral("asice") || ext == QStringLiteral("sce")) exts << asic;
		if(ext == QStringLiteral("edoc")) exts << edoc;
		if(ext == QStringLiteral("adoc")) exts << adoc;
	}
	else
	{
		exts = QStringList({ bdoc, asic, edoc, adoc });
		if(ext == QStringLiteral("bdoc")) active = bdoc;
		if(ext == QStringLiteral("cdoc")) active = cdoc;
		if(ext == QStringLiteral("asice") || ext == QStringLiteral("sce")) active = asic;
		if(ext == QStringLiteral("edoc")) active = edoc;
		if(ext == QStringLiteral("adoc")) active = adoc;
	}

	WaitDialogHider hider;
	return FileDialog::getSaveFileName( this, title, filename, exts.join(QStringLiteral(";;")), &active );
}

void MainWindow::selectPage(Pages page)
{
	selectPageIcon( page < CryptoIntro ? ui->signature : (page == MyEid ? ui->myEid : ui->crypto));
	ui->startScreen->setCurrentIndex(page);
	updateWarnings();
	adjustDrops();
}

void MainWindow::selectPageIcon( PageIcon* page )
{
	ui->rightShadow->raise();
	for( auto pageIcon: { ui->signature, ui->crypto, ui->myEid } )
	{
		pageIcon->activate( pageIcon == page );
	}
}

void MainWindow::showCardMenu(bool show)
{
	if(show)
	{
		const TokenData &t = qApp->signer()->tokensign().cert().isNull() ? qApp->signer()->tokenauth()
			: qApp->signer()->tokensign();

		cardPopup.reset(new CardPopup(cards(), t.card(), qApp->signer()->cache(), this));
		// To select active card from several cards in readers ..
		connect(cardPopup.get(), &CardPopup::activated, qApp->smartcard(), &QSmartCard::selectCard, Qt::QueuedConnection);
		connect(cardPopup.get(), &CardPopup::activated, qApp->signer(), &QSigner::selectCard, Qt::QueuedConnection);
		// .. and hide card popup menu
		connect(cardPopup.get(), &CardPopup::activated, this, &MainWindow::hideCardPopup);
		cardPopup->show();
	}
	else if(cardPopup)
	{
		cardPopup->close();
	}
}

void MainWindow::showCardStatus()
{
	Application::restoreOverrideCursor();
	TokenData st = qApp->signer()->tokensign();
	TokenData at = qApp->signer()->tokenauth();
	const TokenData &t = st.cert().isNull() ? at : st;

	closeWarnings(MyEid);

	if(!t.card().isEmpty() && !t.cert().isNull())
	{
		ui->idSelector->show();
		ui->infoStack->show();
		ui->accordion->show();
		ui->noCardInfo->hide();

		qCDebug(MLog) << "Select card" << t.card();
		auto cardInfo = qApp->signer()->cache()[t.card()];

		if(ui->cardInfo->id() != t.card())
		{
			ui->infoStack->clearData();
			ui->accordion->clear();
			clearWarning({WarningType::UpdateCertWarning});
		}

		ui->cardInfo->update(cardInfo, t.card());

		const SslCertificate &authCert = at.cert();
		const SslCertificate &signCert = st.cert();
		bool seal = cardInfo->type & SslCertificate::TempelType;

		// Card (e.g. e-Seal) can have only one cert
		if(!signCert.isNull())
			emit ui->signContainerPage->cardChanged(cardInfo->id, seal, !signCert.isValid());
		else
			emit ui->signContainerPage->cardChanged();
		if(!authCert.isNull())
			emit ui->cryptoContainerPage->cardChanged(cardInfo->id, seal, !authCert.isValid(), authCert.QSslCertificate::serialNumber());
		else
			emit ui->cryptoContainerPage->cardChanged();
		if(cryptoDoc)
			ui->cryptoContainerPage->update(cryptoDoc->canDecrypt(authCert));

		if(cardInfo->type & SslCertificate::TempelType)
		{
			ui->infoStack->update(*cardInfo);
			ui->accordion->updateInfo(*cardInfo, authCert, signCert);
			ui->myEid->invalidIcon((!authCert.isNull() && !authCert.isValid()) || 
				(!signCert.isNull() && !signCert.isValid()));
			updateCardWarnings();
 		}
	}
	else
	{
		emit ui->signContainerPage->cardChanged();
		emit ui->cryptoContainerPage->cardChanged();

		closeWarnings(MyEid);
		clearWarning({CertExpiredWarning, CertExpiryWarning, UnblockPin1Warning, UnblockPin2Warning, UpdateCertWarning});
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
	selector->setVisible(cards().size() > 1);
	ui->cardInfo->setCursor(selector->isVisible() ? Qt::PointingHandCursor : Qt::ArrowCursor);
	if(selector->isHidden())
		hideCardPopup();
}

void MainWindow::showEvent(QShowEvent * /*event*/)
{
	static int height = 94;
	static int width = 166;

	if(!qApp->initialized())
	{
		FadeInNotification* notification = new FadeInNotification(this, WHITE, NONE,
			QPoint(this->width() - width - 15, this->height() - height - 70), width, height);
		QSvgWidget* structureFunds = new QSvgWidget(QStringLiteral(":/images/Struktuurifondid.svg"), notification);
		structureFunds->resize(width, height);
		structureFunds->show();
		notification->start(QString(), 400, 4000, 1100);
	}
}

void MainWindow::showOverlay( QWidget *parent )
{
	overlay.reset( new Overlay(parent) );
	overlay->show();
}

void MainWindow::showSettings(int page)
{
	QSmartCardData t = qApp->smartcard()->data();
	QString appletVersion = t.isNull() ? QString() : t.appletVersion();
	SettingsDialog dlg(page, this, appletVersion);

	connect(&dlg, &SettingsDialog::langChanged, this, [this](const QString& lang ) {
		qApp->loadTranslation( lang );
		ui->retranslateUi(this);
	});
	connect(&dlg, &SettingsDialog::removeOldCert, this,	&MainWindow::removeOldCert);
	connect(&dlg, &SettingsDialog::togglePrinting, ui->signContainerPage, &ContainerPage::togglePrinting);
	dlg.exec();
}

bool MainWindow::sign()
{
	CheckConnection connection;
	if(!connection.check(QStringLiteral("https://id.eesti.ee/config.json")))
	{
		showWarning(WarningText(WarningType::CheckConnectionWarning));
		return false;
	}

	AccessCert access(this);
	if( !access.validate() )
		return false;

	QString role, city, state, country, zip;
	if(Settings(qApp->applicationName()).value(QStringLiteral("Client/RoleAddressInfo"), false).toBool())
	{
		RoleAddressDialog dlg(this);
		if(dlg.exec() == QDialog::Rejected)
			return false;
		role = dlg.role();
		city = dlg.city();
		state = dlg.state();
		country = dlg.country();
		zip = dlg.zip();
	}

	WaitDialogHolder waitDialog(this, tr("Signing"));
	if(digiDoc->sign(city, state, zip, country, role, QString()))
	{
		access.increment();
		if(save())
		{
			ui->signContainerPage->transition(digiDoc);
			waitDialog.close();

			FadeInNotification* notification = new FadeInNotification(this, WHITE, MANTIS, 110);
			notification->start(tr("The container has been successfully signed!"), 750, 3000, 1200);
			adjustDrops();
			return true;
		}
	}
	else if((qApp->signer()->tokensign().flags() & TokenData::PinLocked))
	{
		qApp->smartcard()->reload();
		showPinBlockedWarning(qApp->smartcard()->data());
	}

	return false;
}

bool MainWindow::signMobile(const QString &idCode, const QString &phoneNumber)
{
	CheckConnection connection;
	if(!connection.check(QStringLiteral("https://id.eesti.ee/config.json")))
	{
		showWarning(WarningText(WarningType::CheckConnectionWarning));
		return false;
	}

	AccessCert access(this);
	if( !access.validate() )
		return false;

	QString role, city, state, country, zip;
	if(Settings(qApp->applicationName()).value(QStringLiteral("Client/RoleAddressInfo"), false).toBool())
	{
		RoleAddressDialog dlg(this);
		if(dlg.exec() == QDialog::Rejected)
			return false;
		role = dlg.role();
		city = dlg.city();
		state = dlg.state();
		country = dlg.country();
		zip = dlg.zip();
	}

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
		adjustDrops();
		return true;
	}

	return false;
}

void MainWindow::updateCardData()
{
	if(cardPopup)
		cardPopup->update(qApp->signer()->cache());
}

void MainWindow::updateMyEid()
{
	Application::restoreOverrideCursor();
	QSmartCardData t = qApp->smartcard()->data();

	if(!t.card().isEmpty() && (!t.authCert().isNull() || !t.signCert().isNull()))
	{
		ui->infoStack->update(t);
		ui->accordion->updateInfo(qApp->smartcard());
		ui->myEid->invalidIcon(!t.authCert().isValid() || !t.authCert().isValid());
		updateCardWarnings();
		showIdCardAlerts(t);
		showPinBlockedWarning(t);
	}
}

void MainWindow::noReader_NoCard_Loading_Event(NoCardInfo::Status status)
{
	qCDebug(MLog) << "noReader_NoCard_Loading_Event" << status;
	ui->idSelector->hide();
	if(status == NoCardInfo::Loading)
		Application::setOverrideCursor(Qt::BusyCursor);

	ui->noCardInfo->show();
	ui->noCardInfo->update(status);
	ui->infoStack->clearData();
	ui->cardInfo->clearPicture();
	ui->infoStack->clearPicture();
	ui->version->setProperty("PICTURE", QVariant());
	ui->infoStack->hide();
	ui->accordion->hide();
	ui->accordion->clearOtherEID();
	ui->myEid->invalidIcon( false );
	ui->myEid->warningIcon( false );
	closeWarnings(MyEid);
	clearWarning({CertExpiredWarning, CertExpiryWarning, UnblockPin1Warning, UnblockPin2Warning, UpdateCertWarning});
}

// Loads picture
void MainWindow::photoClicked( const QPixmap *photo )
{
	if( photo )
		return savePhoto();

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

	QPixmap pixmap = QPixmap::fromImage( image );
	ui->cardInfo->showPicture( pixmap );
	ui->infoStack->showPicture( pixmap );
	ui->version->setProperty("PICTURE", buffer);
}

void MainWindow::removeAddress(int index)
{
	if(cryptoDoc)
	{
		cryptoDoc->removeKey(index);
		ui->cryptoContainerPage->transition(cryptoDoc, cryptoDoc->canDecrypt(qApp->signer()->tokenauth().cert()));
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
		adjustDrops();
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

void MainWindow::savePhoto()
{
	if(!ui->version->property("PICTURE").isValid())
		return;

	QString fileName = QStandardPaths::writableLocation(QStandardPaths::PicturesLocation);
	fileName += "/" + qApp->signer()->tokenauth().card();
	fileName = QFileDialog::getSaveFileName(this,
		tr("Save photo"), fileName,
		tr("Photo (*.jpg *.jpeg);;All Files (*)"));

	if(fileName.isEmpty())
		return;
	static const QStringList exts{QStringLiteral("jpg"), QStringLiteral("jpeg")};
	if(!exts.contains(QFileInfo(fileName).suffix(), Qt::CaseInsensitive))
		fileName.append(QStringLiteral(".jpg"));
	QByteArray pix = ui->version->property("PICTURE").toByteArray();
	QFile f(fileName);
	if(!f.open(QFile::WriteOnly) || f.write(pix) != pix.size())
		showWarning(DocumentModel::tr("Failed to save file '%1'").arg(fileName));
}

void MainWindow::showUpdateCertWarning(const QString &readerName)
{
	emit ui->accordion->showCertWarnings();
	showWarning(WarningText(WarningType::UpdateCertWarning, QString(), readerName));
}

void MainWindow::showWarning(const WarningText &warningText)
{
	if(warningText.warningType)
	{
		for(auto warning: warnings)
		{
			if(warning->warningType() == warningText.warningType)
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

	if(!QFileInfo::exists(fileName))
		return;
	q.addQueryItem(QStringLiteral("subject"), QFileInfo(fileName).fileName() );
	q.addQueryItem(QStringLiteral("attachment"), QFileInfo(fileName).absoluteFilePath() );
	url.setScheme(QStringLiteral("mailto"));
	url.setQuery(q);
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
		ui->topBarShadow->setStyleSheet(QStringLiteral("background: qlineargradient(x1: 0, y1: 0, x2: 0, y2: 1, stop: 0 #c8c8c8, stop: 1 #F4F5F6); \nborder: none;"));
	else
		ui->topBarShadow->setStyleSheet(QStringLiteral("background: qlineargradient(x1: 0, y1: 0, x2: 0, y2: 1, stop: 0 #b5aa92, stop: 1 #F8DDA7); \nborder: none;"));

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
	for(const auto &file: files)
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
	if(link.startsWith(QStringLiteral("#update-Certificate-")))
		updateCertificate(link.mid(20));
	else if(link.startsWith(QStringLiteral("#invalid-signature-")))
		emit ui->signContainerPage->details(link.mid(19));
	else if(link == QStringLiteral("#unblock-PIN1"))
		ui->accordion->changePin1Clicked (false, true);
	else if(link == QStringLiteral("#unblock-PIN2"))
		ui->accordion->changePin2Clicked (false, true);
	else if(link.startsWith(QStringLiteral("http")))
		QDesktopServices::openUrl(QUrl(link));
}

bool MainWindow::wrap(const QString& wrappedFile, bool enclose)
{
	QString ext = Settings(qApp->applicationName()).value(QStringLiteral("Client/Type"), QStringLiteral("bdoc")).toString();
	QString filename = FileUtil::create(QFileInfo(wrappedFile), QStringLiteral(".%1").arg(ext), tr("signature container"));
	if(filename.isNull())
		return false;

	std::unique_ptr<DigiDoc> signatureContainer(new DigiDoc(this));
	signatureContainer->create(filename);

	// If encrypted container or pdf, add whole file to signature container; otherwise content only
	if(enclose)
		signatureContainer->documentModel()->addFile(wrappedFile);
	else
		signatureContainer->documentModel()->addTempFiles(cryptoDoc->documentModel()->tempFiles());

	resetCryptoDoc();
	resetDigiDoc(signatureContainer.release());

	ui->signContainerPage->transition(digiDoc);
	selectPage(SignDetails);

	return true;
}

void MainWindow::wrapAndSign()
{
	showOverlay(this);
	QString wrappedFile = digiDoc->fileName();
	if(!wrap(wrappedFile, true))
	{
		clearOverlay();
		return;
	}

	if(!sign())
	{
		resetDigiDoc(nullptr, false);
		navigateToPage(SignDetails, {wrappedFile}, false);
	}

	clearOverlay();
}

void MainWindow::wrapAndMobileSign(const QString &idCode, const QString &phoneNumber)
{
	showOverlay(this);
	QString wrappedFile = digiDoc->fileName();
	if(!wrap(wrappedFile, true))
	{
		clearOverlay();
		return;
	}

	if(!signMobile(idCode, phoneNumber))
	{
		resetDigiDoc(nullptr, false);
		navigateToPage(SignDetails, {wrappedFile}, false);
	}

	clearOverlay();
}

bool MainWindow::wrapContainer(bool signing)
{
	QString msg = signing ?
		tr("Files can not be added to the signed container. The system will create a new container which shall contain the signed document and the files you wish to add.") :
		tr("Files can not be added to the cryptocontainer. The system will create a new container which shall contain the cypto-document and the files you wish to add.");
	WarningDialog dlg(msg, this);
	dlg.setCancelText(tr("CANCEL"));
	dlg.addButton(tr("CONTINUE"), ContainerSave);
	dlg.exec();
	return dlg.result() == ContainerSave;
}

void MainWindow::showIdCardAlerts(const QSmartCardData& t)
{
	if(qApp->smartcard()->property( "lastcard" ).toString() != t.card() &&
		t.version() == QSmartCardData::VER_3_4 &&
		(!t.authCert().validateEncoding() || !t.signCert().validateEncoding()))
	{
		qApp->showWarning( tr("Your ID-card certificates cannot be renewed starting from 01.07.2017."));
	}
	qApp->smartcard()->setProperty("lastcard", t.card());

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
	bool isBlockedPuk = t.retryCount( QSmartCardData::PukType ) == 0;

	if(	!isBlockedPuk && t.retryCount( QSmartCardData::Pin2Type ) == 0 )
	{
		showWarning(WarningText(WarningType::UnblockPin2Warning));
		emit ui->signContainerPage->cardChanged(); // hide Sign button
	}
	if(	!isBlockedPuk && t.retryCount( QSmartCardData::Pin1Type ) == 0 )
	{
		showWarning(WarningText(WarningType::UnblockPin1Warning));
		emit ui->cryptoContainerPage->cardChanged(); // hide Decrypt button
	}
}

void MainWindow::updateKeys(const QList<CKey> &keys)
{
	if(!cryptoDoc)
		return;

	for(int i = cryptoDoc->keys().size() - 1; i >= 0; i--)
		cryptoDoc->removeKey(i);
	for(const auto &key: keys)
		cryptoDoc->addKey(key);
	ui->cryptoContainerPage->update(cryptoDoc->canDecrypt(qApp->signer()->tokenauth().cert()), cryptoDoc);
}

void MainWindow::containerSummary()
{
#ifdef Q_OS_WIN
	if( QPrinterInfo::availablePrinterNames().isEmpty() )
	{
		qApp->showWarning(
			tr("In order to view Validity Confirmation Sheet there has to be at least one printer installed!") );
		return;
	}
#endif
	QPrintPreviewDialog *dialog = new QPrintPreviewDialog( this );
	dialog->printer()->setPaperSize( QPrinter::A4 );
	dialog->printer()->setOrientation( QPrinter::Portrait );
	dialog->setMinimumHeight( 700 );
	connect(dialog, &QPrintPreviewDialog::paintRequested, digiDoc, [=](QPrinter *printer){
		PrintSheet(digiDoc, printer);
	});
	dialog->exec();
	dialog->deleteLater();
}
