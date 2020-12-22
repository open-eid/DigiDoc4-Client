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
#include "CheckConnection.h"
#include "Colors.h"
#include "DigiDoc.h"
#include "PrintSheet.h"
#include "QPCSC.h"
#include "QSigner.h"
#include "Styles.h"
#include "TokenData.h"
#include "XmlReader.h"
#include "crypto/CryptoDoc.h"
#include "effects/ButtonHoverFilter.h"
#include "effects/FadeInNotification.h"
#include "effects/Overlay.h"
#include "dialogs/AccessCert.h"
#include "dialogs/FileDialog.h"
#include "dialogs/MobileProgress.h"
#include "dialogs/RoleAddressDialog.h"
#include "dialogs/SettingsDialog.h"
#include "dialogs/SmartIDProgress.h"
#include "dialogs/WaitDialog.h"
#include "dialogs/WarningDialog.h"
#include "widgets/DropdownButton.h"
#include "widgets/CardPopup.h"
#include "widgets/VerifyCert.h"
#include "widgets/WarningItem.h"
#include "widgets/WarningList.h"

#include <QLoggingCategory>
#include <QMessageBox>
#include <QMimeData>
#include <QtCore/QUrlQuery>
#include <QtCore/QSettings>
#include <QtGui/QDesktopServices>
#include <QtPrintSupport/QPrinterInfo>
#include <QtPrintSupport/QPrintPreviewDialog>

using namespace ria::qdigidoc4;
using namespace ria::qdigidoc4::colors;

Q_LOGGING_CATEGORY(MLog, "qdigidoc4.MainWindow")

MainWindow::MainWindow( QWidget *parent )
	: QWidget( parent )
	, ui( new Ui::MainWindow )
	, warnings(new WarningList(ui, this))
{
	setAttribute(Qt::WA_DeleteOnClose, true);
	setAcceptDrops( true );
	ui->setupUi(this);

	QFont version = Styles::font(Styles::Regular, 12);
	version.setUnderline(true);
	QFont condensed11 = Styles::font( Styles::Condensed, 11 );
	QFont condensed14 = Styles::font( Styles::Condensed, 14 );
	QFont regular20 = Styles::font( Styles::Regular, 20 );
	ui->version->setFont(version);
	ui->signIntroLabel->setFont( regular20 );
	ui->signIntroButton->setFont( condensed14 );
	ui->cryptoIntroLabel->setFont( regular20 );
	ui->cryptoIntroButton->setFont( condensed14 );
	ui->noCardInfo->setFont(condensed14);
	ui->noReaderInfoText->setFont(regular20);
	ui->noReaderInfoText->setProperty("currenttext", ui->noReaderInfoText->text());
	ui->help->setFont( condensed11 );
	ui->settings->setFont( condensed11 );

	connect(warnings, &WarningList::warningClicked, this, &MainWindow::warningClicked);

	ui->version->setText(QStringLiteral("%1%2").arg(tr("Ver. "), qApp->applicationVersion()));
	connect(ui->version, &QPushButton::clicked, this, [this] {showSettings(SettingsDialog::DiagnosticsSettings);});

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

	ui->help->installEventFilter(new ButtonHoverFilter(QStringLiteral(":/images/icon_Abi.svg"), QStringLiteral(":/images/icon_Abi_hover.svg"), this));
	ui->settings->installEventFilter(new ButtonHoverFilter(QStringLiteral(":/images/icon_Seaded.svg"), QStringLiteral(":/images/icon_Seaded_hover.svg"), this));
	connect(ui->help, &QToolButton::clicked, qApp, &Application::openHelp);
	connect(ui->settings, &QToolButton::clicked, this, [this] { showSettings(SettingsDialog::GeneralSettings); });

#ifdef Q_OS_WIN
	// Add grey line on Windows in order to separate white title bar from card selection bar
	QWidget *separator = new QWidget(this);
	separator->setFixedHeight(1);
	separator->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
	separator->setStyleSheet(QStringLiteral("background-color: #D9D9D9;"));
	separator->resize(8000, 1);
	separator->move(110, 0);
	separator->show();
#endif

	// Refresh ID card info in card widget
	connect(qApp->signer(), &QSigner::cacheChanged, this, &MainWindow::updateSelector);
	connect(qApp->signer(), &QSigner::signDataChanged, this, [=](const TokenData &token){
		updateSelector();
		updateMyEID(token);
		ui->signContainerPage->cardChanged(token.cert());
	});
	connect(qApp->signer(), &QSigner::authDataChanged, this, [=](const TokenData &token){
		updateSelector();
		updateMyEID(token);
		ui->cryptoContainerPage->cardChanged(token.cert());
		if(cryptoDoc)
			ui->cryptoContainerPage->update(cryptoDoc->canDecrypt(token.cert()));
	});

	// Refresh card info on "My EID" page
	connect(qApp->signer()->smartcard(), &QSmartCard::dataChanged, this, &MainWindow::updateMyEid);
	// Show card pop-up menu
	connect(ui->selector->selector, &DropdownButton::dropdown, this, &MainWindow::showCardMenu);

	connect(this, &MainWindow::clearPopups, ui->signContainerPage, &ContainerPage::clearPopups);
	connect(this, &MainWindow::clearPopups, this, [this] { showCardMenu(false); });

	connect(ui->signIntroButton, &QPushButton::clicked, this, [this] { openContainer(true); });
	connect(ui->cryptoIntroButton, &QPushButton::clicked, this, [this] { openContainer(false); });
	connect(ui->signContainerPage, &ContainerPage::action, this, &MainWindow::onSignAction);
	connect(ui->signContainerPage, &ContainerPage::addFiles, this, [this](const QStringList &files) { openFiles(files, true); } );
	connect(ui->signContainerPage, &ContainerPage::fileRemoved, this, &MainWindow::removeSignatureFile);
	connect(ui->signContainerPage, &ContainerPage::removed, this, &MainWindow::removeSignature);
	connect(ui->signContainerPage, &ContainerPage::warning, this, [this](const WarningText &warningText) {
		warnings->showWarning(warningText);
		ui->signature->warningIcon(true);
	});

	connect(ui->cryptoContainerPage, &ContainerPage::action, this, &MainWindow::onCryptoAction);
	connect(ui->cryptoContainerPage, &ContainerPage::addFiles, this, [this](const QStringList &files) { openFiles(files, true); } );
	connect(ui->cryptoContainerPage, &ContainerPage::fileRemoved, this, &MainWindow::removeCryptoFile);
	connect(ui->cryptoContainerPage, &ContainerPage::keysSelected, this, &MainWindow::updateKeys);
	connect(ui->cryptoContainerPage, &ContainerPage::removed, this, &MainWindow::removeAddress);

	connect(ui->accordion, &Accordion::changePin1Clicked, this, &MainWindow::changePin1Clicked);
	connect(ui->accordion, &Accordion::changePin2Clicked, this, &MainWindow::changePin2Clicked);
	connect(ui->accordion, &Accordion::changePukClicked, this, &MainWindow::changePukClicked);
	connect(ui->accordion, &Accordion::checkEMail, this, &MainWindow::getEmailStatus);   // To check e-mail
	connect(ui->accordion, &Accordion::activateEMail, this, &MainWindow::activateEmail);   // To activate e-mail
	connect(ui->infoStack, &InfoStack::photoClicked, this, &MainWindow::photoClicked);
	connect(ui->cardInfo, &CardWidget::photoClicked, this, &MainWindow::photoClicked);   // To load photo
	connect(ui->cardInfo, &CardWidget::selected, ui->selector->selector, &DropdownButton::press);

	updateSelector();
	updateMyEID(qApp->signer()->tokensign());
	updateMyEid(qApp->signer()->smartcard()->data());
	ui->signContainerPage->cardChanged(qApp->signer()->tokensign().cert());
	ui->cryptoContainerPage->cardChanged(qApp->signer()->tokenauth().cert());
}

MainWindow::~MainWindow()
{
	delete ui;
}

void MainWindow::pageSelected(PageIcon *page)
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
	setAcceptDrops(currentState() != SignedContainer);
}

void MainWindow::browseOnDisk( const QString &fileName )
{
	if(!QFileInfo::exists(fileName))
		return;
	QUrl url = QUrl::fromLocalFile( fileName );
	url.setScheme(QStringLiteral("browse"));
	QDesktopServices::openUrl( url );
}

void MainWindow::changeEvent(QEvent* event)
{
	if (event->type() == QEvent::LanguageChange)
	{
		ui->retranslateUi(this);
		ui->noReaderInfoText->setText(tr(ui->noReaderInfoText->property("currenttext").toByteArray()));
		ui->version->setText(QStringLiteral("%1%2").arg(tr("Ver. "), qApp->applicationVersion()));
		setWindowTitle(windowFilePath().isEmpty() ? tr("DigiDoc4 client") : QFileInfo(windowFilePath()).fileName());
		showCardMenu(false);
	}
	QWidget::changeEvent(event);
}

void MainWindow::closeEvent(QCloseEvent * /*event*/)
{
	resetCryptoDoc();
	resetDigiDoc();
	ui->startScreen->setCurrentIndex(SignIntro);
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

	WaitDialogHolder waitDialog(this, tr("Decrypting"));

	return cryptoDoc->decrypt();
}

QString MainWindow::digiDocPath()
{
	return digiDoc ? digiDoc->fileName() : QString();
}

void MainWindow::dragEnterEvent(QDragEnterEvent *event)
{
	if(!event->source() && !dropEventFiles(event).isEmpty())
	{
		event->acceptProposedAction();
		if(!findChild<Overlay*>())
			new Overlay(this, this);
	}
	else
		event->ignore();
}

void MainWindow::dragLeaveEvent(QDragLeaveEvent *event)
{
	event->accept();
	for(Overlay *overlay: findChildren<Overlay*>())
		overlay->deleteLater();
}

void MainWindow::dropEvent(QDropEvent *event)
{
	for(Overlay *overlay: findChildren<Overlay*>())
		overlay->deleteLater();
	QStringList files = dropEventFiles(event);
	if(!files.isEmpty())
	{
		event->acceptProposedAction();
		openFiles(files, true);
	}
	else
		event->ignore();
}

QStringList MainWindow::dropEventFiles(QDropEvent *event) const
{
	QStringList files;
	const QList<QUrl> urls = event->mimeData()->urls();
	for(const auto &url: urls)
	{
		if(url.scheme() == QStringLiteral("file") && QFileInfo(url.toLocalFile()).isFile())
			files << url.toLocalFile();
	}
	return files;
}

bool MainWindow::encrypt()
{
	if(!cryptoDoc)
		return false;

	if(!FileDialog::fileIsWritable(cryptoDoc->fileName())) {
		WarningDialog dlg(tr("Cannot alter container %1. Save different location?").arg(cryptoDoc->fileName()), this);
		dlg.addButton(tr("YES").toUpper(), QMessageBox::Yes);
		if(dlg.exec() == QMessageBox::Yes) {
			moveCryptoContainer();
			return encrypt();
		}
		return false;
	}

	WaitDialogHolder waitDialog(this, tr("Encrypting"));

	return cryptoDoc->encrypt();
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
		QWidget::mouseReleaseEvent(event);
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
			QString defaultDir = QSettings().value(QStringLiteral("DefaultDir")).toString();
			QString filename = FileDialog::createNewFileName(files[0], QStringLiteral(".asice"), tr("signature container"), defaultDir);
			if(!filename.isNull())
			{
				signatureContainer->create(filename);
				bool filesAdded = false;
				for(const auto &file: files)
				{
					if(signatureContainer->documentModel()->addFile(file))
						filesAdded = true;
				}
				navigate = filesAdded;
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
			QString defaultDir = QSettings().value(QStringLiteral("DefaultDir")).toString();
			QString filename = FileDialog::createNewFileName(files[0], QStringLiteral(".cdoc"), tr("crypto container"), defaultDir);
			if(!filename.isNull())
			{
				cryptoContainer->clear(filename);
				bool filesAdded = false;
				for(const auto &file: files)
				{
					if(cryptoContainer->documentModel()->addFile(file))
						filesAdded = true;
				}
				navigate = filesAdded;
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
		sign([this](const QString &city, const QString &state, const QString &zip, const QString &country, const QString &role) {
			return digiDoc->sign(city, state, zip, country, role, qApp->signer());
		});
		break;
	case SignatureMobile:
		sign([this, info1, info2](const QString &city, const QString &state, const QString &zip, const QString &country, const QString &role) {
			MobileProgress m(this);
			return m.init(info1, info2) &&
				digiDoc->sign(city, state, zip, country, role, &m);
		});
		break;
	case SignatureSmartID:
		sign([this, info1, info2](const QString &city, const QString &state, const QString &zip, const QString &country, const QString &role) {
			SmartIDProgress s(this);
			return s.init(info1, info2) &&
				digiDoc->sign(city, state, zip, country, role, &s);
		});
		break;
	case ClearSignatureWarning:
		ui->signature->warningIcon(false);
		warnings->closeWarnings(SignDetails);
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
	QString defaultDir = QSettings().value(QStringLiteral("Client/DefaultDir")).toString();
	QString filename = FileDialog::createNewFileName(digiDoc->fileName(), QStringLiteral(".cdoc"), tr("crypto container"), defaultDir);
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
	QString to = selectFile(tr("Move file"), cryptoDoc->fileName(), true);
	if(!to.isNull() && cryptoDoc->move(to))
		emit ui->cryptoContainerPage->moved(to);
}

void MainWindow::moveSignatureContainer()
{
	QString to = selectFile(tr("Move file"), digiDoc->fileName(), true);
	if(!to.isNull() && digiDoc->move(to))
		emit ui->signContainerPage->moved(to);
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
	case DecryptToken:
		if(decrypt())
		{
			ui->cryptoContainerPage->transition(cryptoDoc, false);

			FadeInNotification* notification = new FadeInNotification( this, WHITE, MANTIS, 110 );
			notification->start( tr("Decryption succeeded"), 750, 3000, 1200 );
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
		QString target = selectFile(tr("Save file"), cryptoDoc->fileName(), true);
		if(target.isEmpty())
			break;
		if( !FileDialog::fileIsWritable(target))
		{
			WarningDialog dlg(tr("Cannot alter container %1. Save different location?").arg(target), this);
			dlg.addButton(tr("YES").toUpper(), QMessageBox::Yes);
			if(dlg.exec() == QMessageBox::Yes) {
				QString file = selectFile(tr("Save file"), target, true);
				if(!file.isEmpty())
					cryptoDoc->saveCopy(file);
			}
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
			auto fileType = FileDialog::detect(content[0]);
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
				content.insert(content.begin(), digiDoc->fileName());
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
		navigateToPage(page, content, create);
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

void MainWindow::openContainer(bool signature)
{
	QString filter = QFileDialog::tr("All Files (*)") + QStringLiteral(";;") + tr("Documents (%1)");
	if(signature)
		filter = filter.arg(
			QStringLiteral("*.bdoc *.ddoc *.asice *.sce *.asics *.scs *.edoc *.adoc") +
			(qApp->confValue(Application::SiVaUrl).toString().isEmpty() ? QString() : QStringLiteral(" *.pdf")));
	else
		filter = filter.arg(QStringLiteral("*.cdoc"));
	QStringList files = FileDialog::getOpenFileNames(this, tr("Select documents"), {}, filter);
	if(!files.isEmpty())
		openFiles(files);
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
		if(dlg.exec() == ContainerSave)
			save();
	}

	ui->signature->warningIcon(false);

	ui->signContainerPage->clear();
	delete digiDoc;
	warnings->closeWarnings(SignDetails);
	digiDoc = doc;
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
	if(target.isEmpty())
		return false;

	if(!FileDialog::fileIsWritable(target))
	{
		WarningDialog dlg(tr("Cannot alter container %1. Save different location?").arg(target), this);
		dlg.addButton(tr("YES").toUpper(), QMessageBox::Yes);
		if(dlg.exec() == QMessageBox::Yes) {
			QString file = selectFile(tr("Save file"), target, true);
			if(!file.isEmpty())
				return saveAs ? digiDoc->saveAs(file) : digiDoc->save(file);
		}
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
	const QString ext = QFileInfo( filename ).suffix().toLower();
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
	warnings->updateWarnings();
	adjustDrops();
	updateSelector();
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
		CardPopup *cardPopup = new CardPopup(ui->selector->list, this);
		connect(cardPopup, &CardPopup::activated, qApp->signer(), &QSigner::selectCard, Qt::QueuedConnection);
		connect(cardPopup, &CardPopup::activated, this, [this] { showCardMenu(false); }); // .. and hide card popup menu
		cardPopup->show();
	}
	else if(CardPopup *cardPopup = findChild<CardPopup*>()) {
		ui->selector->selector->init();
		cardPopup->deleteLater();
	}
}

void MainWindow::showEvent(QShowEvent * /*event*/)
{
	static bool isShown = false;
	if(isShown)
		return;
	static const int height = 94;
	static const int width = 166;
	FadeInNotification* notification = new FadeInNotification(this, WHITE, NONE,
		QPoint(this->width() - width - 15, this->height() - height - 70), width, height);
	QSvgWidget* structureFunds = new QSvgWidget(QStringLiteral(":/images/Struktuurifondid.svg"), notification);
	structureFunds->resize(width, height);
	structureFunds->show();
	notification->start({}, 400, 4000, 1100);
	isShown = true;
}

void MainWindow::showSettings(int page)
{
	if(SettingsDialog *settings = findChild<SettingsDialog*>())
	{
		settings->showPage(page);
		settings->show();
		return;
	}
	SettingsDialog dlg(page, this);

	connect(&dlg, &SettingsDialog::langChanged, this, [this](const QString& lang ) {
		qApp->loadTranslation( lang );
		ui->retranslateUi(this);
	});
	connect(&dlg, &SettingsDialog::togglePrinting, ui->signContainerPage, &ContainerPage::togglePrinting);
	dlg.exec();
}

void MainWindow::sign(const std::function<bool(const QString &city, const QString &state, const QString &zip, const QString &country, const QString &role)> &sign)
{
	if(!CheckConnection().check(QStringLiteral("https://id.eesti.ee/config.json")))
	{
		FadeInNotification *notification = new FadeInNotification(this, MOJO, MARZIPAN, 110);
		notification->start(tr("Check internet connection"), 750, 3000, 1200);
		return;
	}

	AccessCert access(this);
	if(!access.validate())
		return;

	QString role, city, state, country, zip;
	if(RoleAddressDialog(this).get(city, country, state, zip, role) == QDialog::Rejected)
		return;

	WaitDialogHolder waitDialog(this, tr("Signing"));
	if(digiDoc->isService())
	{
		QString wrappedFile = digiDoc->fileName();
		if(!wrap(wrappedFile, true))
			return;

		if(!sign(city, state, zip, country, role))
		{
			resetDigiDoc(nullptr, false);
			navigateToPage(SignDetails, {wrappedFile}, false);
			return;
		}
	}
	else if(!sign(city, state, zip, country, role))
		return;

	access.increment();
	if(!save())
		return;

	waitDialog.close();
	ui->signContainerPage->transition(digiDoc);

	FadeInNotification* notification = new FadeInNotification(this, WHITE, MANTIS, 110);
	notification->start(tr("The container has been successfully signed!"), 750, 3000, 1200);
	adjustDrops();
}

// Loads picture
void MainWindow::photoClicked(const QPixmap &photo)
{
	if(!photo.isNull())
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
			warnings->showWarning(WarningText(XmlReader::emailErr(error.toUInt())));
		return;
	}

	QPixmap pixmap = QPixmap::fromImage( image );
	ui->cardInfo->showPicture( pixmap );
	ui->infoStack->showPicture( pixmap );
	ui->infoStack->setProperty("PICTURE", buffer);
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
	auto count = model->rowCount();
	if(count != 1)
	{
		model->removeRows(index, 1);
	}
	else
	{
		WarningDialog dlg(tr("You are about to delete the last file in the container, it is removed along with the container."), this);
		dlg.setCancelText(tr("CANCEL"));
		dlg.resetCancelStyle();
		dlg.addButton(tr("REMOVE"), ContainerSave, true);
		if (dlg.exec() == ContainerSave) {
			window()->setWindowFilePath({});
			window()->setWindowTitle(tr("DigiDoc4 client"));
			return true;
		}
	}

	return false;
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
	if(!ui->infoStack->property("PICTURE").isValid())
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
	QByteArray pix = ui->infoStack->property("PICTURE").toByteArray();
	QFile f(fileName);
	if(!f.open(QFile::WriteOnly) || f.write(pix) != pix.size())
		warnings->showWarning(DocumentModel::tr("Failed to save file '%1'").arg(fileName));
}

void MainWindow::containerToEmail( const QString &fileName )
{
	if(!QFileInfo::exists(fileName))
		return;
	QUrlQuery q;
	q.addQueryItem(QStringLiteral("subject"), QFileInfo(fileName).fileName() );
	q.addQueryItem(QStringLiteral("attachment"), QFileInfo(fileName).absoluteFilePath() );
	QUrl url;
	url.setScheme(QStringLiteral("mailto"));
	url.setQuery(q);
	QDesktopServices::openUrl( url );
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
	if(link.startsWith(QStringLiteral("#invalid-signature-")))
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
	QString defaultDir = QSettings().value(QStringLiteral("DefaultDir")).toString();
	QString filename = FileDialog::createNewFileName(wrappedFile, QStringLiteral(".asice"), tr("signature container"), defaultDir);
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

bool MainWindow::wrapContainer(bool signing)
{
	QString msg = signing ?
		tr("Files can not be added to the signed container. The system will create a new container which shall contain the signed document and the files you wish to add.") :
		tr("Files can not be added to the cryptocontainer. The system will create a new container which shall contain the cypto-document and the files you wish to add.");
	WarningDialog dlg(msg, this);
	dlg.setCancelText(tr("CANCEL"));
	dlg.addButton(tr("CONTINUE"), ContainerSave);
	return dlg.exec() == ContainerSave;
}

void MainWindow::updateSelector()
{
	switch(ui->startScreen->currentIndex())
	{
	case SignIntro:
	case SignDetails:
		ui->cardInfo->update(qApp->signer()->tokensign(), ui->selector->list.size() > 1);
		ui->selector->setList(qApp->signer()->tokensign().card(), qApp->signer()->cache(), IDSelector::Signing);
		break;
	case CryptoIntro:
	case CryptoDetails:
		ui->cardInfo->update(qApp->signer()->tokenauth(), ui->selector->list.size() > 1);
		ui->selector->setList(qApp->signer()->tokenauth().card(), qApp->signer()->cache(), IDSelector::Decrypting);
		break;
	case MyEid:
		ui->cardInfo->update(qApp->signer()->smartcard()->tokenData(), ui->selector->list.size() > 1);
		ui->selector->setList(qApp->signer()->smartcard()->tokenData().card(), qApp->signer()->cache(), IDSelector::MyEID);
		break;
	default: break;
	}
	ui->idSelector->setHidden(ui->cardInfo->token().isNull());
	ui->noCardInfo->setVisible(ui->idSelector->isHidden());
	if (!QPCSC::instance().serviceRunning())
		ui->noCardInfo->update(NoCardInfo::NoPCSC);
	else if(QPCSC::instance().readers().isEmpty())
		ui->noCardInfo->update(NoCardInfo::NoReader);
	else
		ui->noCardInfo->update(NoCardInfo::NoCard);
	ui->cardInfo->setCursor(ui->selector->selector->isVisible() ? Qt::PointingHandCursor : Qt::ArrowCursor);
	if(ui->selector->selector->isHidden())
		showCardMenu(false);
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
