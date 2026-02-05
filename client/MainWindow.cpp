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
#include "CryptoDoc.h"
#include "DigiDoc.h"
#include "QPCSC.h"
#include "QSigner.h"
#include "SslCertificate.h"
#include "TokenData.h"
#include "effects/FadeInNotification.h"
#include "effects/Overlay.h"
#include "dialogs/FileDialog.h"
#include "dialogs/MobileProgress.h"
#include "dialogs/PasswordDialog.h"
#include "dialogs/RoleAddressDialog.h"
#include "dialogs/SettingsDialog.h"
#include "dialogs/SmartIDProgress.h"
#include "dialogs/WaitDialog.h"
#include "dialogs/WarningDialog.h"
#include "widgets/CardPopup.h"
#include "widgets/WarningItem.h"
#include "widgets/WarningList.h"

#include <QtCore/QMimeData>
#include <QtGui/QDragEnterEvent>
#include <QtWidgets/QMessageBox>

using namespace ria::qdigidoc4;
using namespace std::chrono;

MainWindow::MainWindow( QWidget *parent )
	: QWidget( parent )
	, ui( new Ui::MainWindow )
{
	setAttribute(Qt::WA_DeleteOnClose, true);
	setAcceptDrops( true );
	ui->setupUi(this);

	ui->signIntroButton->setFocus();
	ui->noReaderInfoText->setProperty("currenttext", ui->noReaderInfoText->text());

	ui->version->setText(QStringLiteral("%1%2").arg(tr("Ver. "), Application::applicationVersion()));
	connect(ui->version, &QPushButton::clicked, this, [this] {showSettings(SettingsDialog::DiagnosticsSettings);});

	ui->coatOfArms->load(QStringLiteral(":/images/Logo_small.svg"));
	ui->pageButtonGroup->setId(ui->signature, Pages::SignIntro);
	ui->pageButtonGroup->setId(ui->crypto, Pages::CryptoIntro);
	ui->pageButtonGroup->setId(ui->myEid, Pages::MyEid);

	connect(ui->pageButtonGroup, &QButtonGroup::idToggled, this, [this](int page, bool checked) {
		if(!checked)
			return;
		if(page == SignIntro && digiDoc)
			page = SignDetails;
		if(page == CryptoIntro && cryptoDoc)
			page = CryptoDetails;
		selectPage(Pages(page));
	});
	connect(ui->help, &QToolButton::clicked, qApp, &Application::openHelp);
	connect(ui->settings, &QToolButton::clicked, this, [this] { showSettings(SettingsDialog::GeneralSettings); });

#ifdef Q_OS_WIN
	// Add grey line on Windows in order to separate white title bar from card selection bar
	QWidget *separator = new QWidget(this);
	separator->setFixedHeight(1);
	separator->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
	separator->setStyleSheet(QStringLiteral("background-color: #D9D9D9;"));
	separator->resize(8000, 1);
	separator->move(mapToGlobal(ui->topBar->pos()));
	separator->show();
#endif

	// Refresh ID card info in card widget
	connect(qApp->signer(), &QSigner::cacheChanged, this, &MainWindow::updateSelector);
	connect(qApp->signer(), &QSigner::signDataChanged, ui->signContainerPage, &ContainerPage::tokenChanged);
	connect(qApp->signer(), &QSigner::authDataChanged, ui->cryptoContainerPage, &ContainerPage::tokenChanged);

	// Refresh card info on "My EID" page
	connect(qApp->signer()->smartcard(), &QSmartCard::tokenChanged, this, &MainWindow::updateMyEID);
	connect(qApp->signer()->smartcard(), &QSmartCard::dataChanged, this, &MainWindow::updateMyEid);

	connect(ui->signIntroButton, &QPushButton::clicked, this, [this] { openContainer(true); });
	connect(ui->cryptoIntroButton, &QPushButton::clicked, this, [this] { openContainer(false); });
	connect(ui->signContainerPage, &ContainerPage::action, this, &MainWindow::onSignAction);
	connect(ui->signContainerPage, &ContainerPage::addFiles, this, [this](const QStringList &files) { openFiles(files); } );
	connect(ui->signContainerPage, &ContainerPage::removed, this, &MainWindow::removeSignature);
	connect(ui->signContainerPage, &ContainerPage::warning, this, [this](WarningText warningText) {
		ui->warnings->showWarning(std::move(warningText));
		ui->signature->warningIcon(true);
	});

	connect(ui->cryptoContainerPage, &ContainerPage::action, this, &MainWindow::onCryptoAction);
	connect(ui->cryptoContainerPage, &ContainerPage::addFiles, this, [this](const QStringList &files) { openFiles(files); } );
	connect(ui->cryptoContainerPage, &ContainerPage::warning, this, [this](WarningText warningText) {
		ui->warnings->showWarning(std::move(warningText));
		ui->crypto->warningIcon(true);
	});

	connect(ui->accordion, &Accordion::changePinClicked, this, &MainWindow::changePinClicked);
	connect(ui->cardInfo, &CardWidget::selected, ui->selector, &QToolButton::toggle);

	ui->signContainerPage->tokenChanged(qApp->signer()->tokensign());
	ui->cryptoContainerPage->tokenChanged(qApp->signer()->tokenauth());
	updateMyEID(qApp->signer()->smartcard()->tokenData());
	updateMyEid(qApp->signer()->smartcard()->data());
}

MainWindow::~MainWindow()
{
	digiDoc.reset();
	cryptoDoc.reset();
	delete ui;
}

void MainWindow::adjustDrops()
{
	setAcceptDrops(currentState() != SignedContainer);
}

void MainWindow::changeEvent(QEvent* event)
{
	if (event->type() == QEvent::LanguageChange)
	{
		ui->retranslateUi(this);
		ui->noReaderInfoText->setText(tr(ui->noReaderInfoText->property("currenttext").toByteArray()));
		ui->version->setText(QStringLiteral("%1%2").arg(tr("Ver. "), Application::applicationVersion()));
		setWindowTitle(windowFilePath().isEmpty() ? tr("DigiDoc4 Client") : FileDialog::normalized(QFileInfo(windowFilePath()).fileName()));
		ui->selector->setChecked(false);
	}
	QWidget::changeEvent(event);
}

void MainWindow::changePinClicked(QSmartCardData::PinType type, QSmartCard::PinAction action)
{
	if(qApp->signer()->smartcard()->pinChange(type, action, ui->topBar))
		updateMyEid(qApp->signer()->smartcard()->data());
}

void MainWindow::closeEvent(QCloseEvent * /*event*/)
{
	cryptoDoc.reset();
	resetDigiDoc({});
	ui->startScreen->setCurrentIndex(SignIntro);
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
		return digiDoc->state();
	return ContainerState::Uninitialized;
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
		openFiles(std::move(files), true);
	}
	else
		event->ignore();
}

QStringList MainWindow::dropEventFiles(QDropEvent *event)
{
	QStringList files;
	const QList<QUrl> urls = event->mimeData()->urls();
	for(const auto &url: urls)
	{
		if(url.scheme() == QLatin1String("file") && QFileInfo(url.toLocalFile()).isFile())
			files << url.toLocalFile();
	}
	return files;
}

bool MainWindow::encrypt(bool askForKey)
{
	if(!cryptoDoc)
		return false;

	if(!FileDialog::fileIsWritable(cryptoDoc->fileName()))
	{
		auto *dlg = WarningDialog::create(this)
			->withTitle(CryptoDoc::tr("Failed to encrypt document"))
			->withText(tr("Cannot alter container %1. Save different location?").arg(FileDialog::normalized(cryptoDoc->fileName())))
			->addButton(WarningDialog::YES, QMessageBox::Yes);
		if(dlg->exec() != QMessageBox::Yes)
			return false;
		QString to = FileDialog::getSaveFileName(this, FileDialog::tr("Save file"), cryptoDoc->fileName());
		if(to.isNull() || !cryptoDoc->move(to))
			return false;
		ui->cryptoContainerPage->setHeader(to);
	}

	if(!askForKey) {
		WaitDialogHolder waitDialog(this, tr("Encrypting"));
		return cryptoDoc->encrypt();
	}

	PasswordDialog p(PasswordDialog::Mode::ENCRYPT, this);
	if(!p.exec())
		return false;

	WaitDialogHolder waitDialog(this, tr("Encrypting"));
	return cryptoDoc->encrypt(cryptoDoc->fileName(), p.label(), p.secret());
}

void MainWindow::mouseReleaseEvent(QMouseEvent *event)
{
	if(auto *cardPopup = findChild<CardPopup*>())
		cardPopup->deleteLater();
	ui->signContainerPage->clearPopups();
	QWidget::mouseReleaseEvent(event);
}

void MainWindow::navigateToPage( Pages page, const QStringList &files, bool create )
{
	bool navigate = true;
	if(page == SignDetails)
	{
		navigate = false;
		auto signatureContainer = std::make_unique<DigiDoc>(this);
		if(create)
		{
			QString filename = FileDialog::createNewFileName(files[0], true, this);
			if(!filename.isNull())
			{
				signatureContainer->create(filename);
				for(const auto &file: files)
				{
					if(signatureContainer->documentModel()->addFile(file))
						navigate = true;
				}
			}
		}
		else
			navigate = signatureContainer->open(files[0]);
		if(navigate)
		{
			resetDigiDoc(std::move(signatureContainer));
			ui->signContainerPage->transition(digiDoc.get());
		}
	}
	else if(page == CryptoDetails)
	{
		navigate = false;
		auto cryptoContainer = std::make_unique<CryptoDoc>(this);
		if(create)
		{
			QString filename = FileDialog::createNewFileName(files[0], false, this);
			if(!filename.isNull())
			{
				cryptoContainer->clear(filename);
				for(const auto &file: files)
				{
					if(cryptoContainer->documentModel()->addFile(file))
						navigate = true;
				}
			}
		}
		else
			navigate = cryptoContainer->open(files[0]);
		if(navigate)
		{
			cryptoDoc = std::move(cryptoContainer);
			ui->cryptoContainerPage->transition(cryptoDoc.get(), qApp->signer()->tokenauth().cert());
		}
	}

	if(navigate)
		selectPage(page);
}

void MainWindow::onSignAction(int action, const QString &idCode, const QString &info2)
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
		sign([this, idCode, info2](const QString &city, const QString &state, const QString &zip, const QString &country, const QString &role) {
			MobileProgress m(this);
			return m.init(idCode, info2) &&
				digiDoc->sign(city, state, zip, country, role, &m);
		});
		break;
	case SignatureSmartID:
		sign([this, idCode, info2](const QString &city, const QString &state, const QString &zip, const QString &country, const QString &role) {
			SmartIDProgress s(this);
			return s.init(info2, idCode, digiDoc->fileName()) &&
				digiDoc->sign(city, state, zip, country, role, &s);
		});
		break;
	case ClearSignatureWarning:
		ui->signature->warningIcon(false);
		ui->warnings->closeWarnings(SignDetails);
		break;
	case ContainerClose:
		digiDoc.reset();
		selectPage(Pages::SignIntro);
		break;
	case ContainerCancel:
		resetDigiDoc({});
		selectPage(Pages::SignIntro);
		break;
	case ContainerConvert:
		if(digiDoc)
			convertToCDoc();
		break;
	default:
		break;
	}
}

void MainWindow::convertToCDoc()
{
	QString filename = FileDialog::createNewFileName(digiDoc->fileName(), false, this);
	if(filename.isNull())
		return;

	auto cryptoContainer = std::make_unique<CryptoDoc>(this);
	cryptoContainer->clear(filename);

	// If signed, add whole signed document to cryptocontainer; otherwise
	// content only
	if (digiDoc->state() == SignedContainer)
		cryptoContainer->documentModel()->addFile(digiDoc->fileName());
	else
		cryptoContainer->documentModel()->addTempFiles(digiDoc->documentModel()->tempFiles());

	auto cardData = qApp->signer()->tokenauth();
	if (!cardData.cert().isNull()) {
		cryptoContainer->addEncryptionKey(cardData.cert());
	}

	cryptoDoc = std::move(cryptoContainer);
	digiDoc.reset();
	ui->cryptoContainerPage->transition(cryptoDoc.get(),  qApp->signer()->tokenauth().cert());
	selectPage(CryptoDetails);

	FadeInNotification::success(ui->topBar, tr("Converted to crypto container!"));
}

void MainWindow::onCryptoAction(int action, const QString &/*id*/, const QString &/*phone*/)
{
	switch(action)
	{
	case ContainerClose:
	case ContainerCancel:
		cryptoDoc.reset();
		selectPage(Pages::CryptoIntro);
		break;
	case ContainerConvert:
		if(cryptoDoc && wrap(cryptoDoc->fileName(), false))
			FadeInNotification::success(ui->topBar, tr("Converted to signed document!"));
		break;
	case DecryptedContainer:
		FadeInNotification::success(ui->topBar, tr("Decryption succeeded!"));
		break;
	case EncryptContainer:
		if(encrypt())
		{
			ui->cryptoContainerPage->transition(cryptoDoc.get(), qApp->signer()->tokenauth().cert());
			FadeInNotification::success(ui->topBar, tr("Encryption succeeded!"));
		}
		break;
	case EncryptLT:
		if(encrypt(true)) {
			ui->cryptoContainerPage->transition(cryptoDoc.get(), qApp->signer()->tokenauth().cert());
			FadeInNotification::success(ui->topBar, tr("Encryption succeeded!"));
		}
		break;
	case ClearCryptoWarning:
		ui->crypto->warningIcon(false);
		ui->warnings->closeWarnings(CryptoDetails);
		break;
	default:
		break;
	}
}

void MainWindow::openFiles(QStringList files, bool addFile, bool forceCreate)
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
	ContainerState state = currentState();
	Pages page = (current == CryptoIntro) ? CryptoDetails : SignDetails;
	bool create = true;
	switch(state)
	{
	case Uninitialized:
	// Case 1.
		if(files.size() == 1)
		{
			auto fileType = FileDialog::detect(files[0]);
			if(current == MyEid)
				page = (fileType == FileDialog::CryptoDocument) ? CryptoDetails : SignDetails;
			create = forceCreate || (
				(fileType != FileDialog::CryptoDocument || page != CryptoDetails) &&
				(fileType != FileDialog::SignatureDocument || page != SignDetails));
		}
		break;
	case ContainerState::UnsignedContainer:
	case ContainerState::UnsignedSavedContainer:
		if(digiDoc->isPDF() && !wrap(digiDoc->fileName(), true))
			return;
		for(DocumentModel* model = digiDoc->documentModel();
			const auto &file: std::as_const(files))
			model->addFile(file);
		selectPage(SignDetails);
		return;
	case ContainerState::UnencryptedContainer:
		for(DocumentModel* model = cryptoDoc->documentModel();
			const auto &file: std::as_const(files))
			model->addFile(file);
		selectPage(CryptoDetails);
		return;
	default:
		if(addFile)
		{
			bool crypto = state & CryptoContainers;
			if(wrapContainer(!crypto))
				files.insert(files.begin(), digiDoc->fileName());
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

	navigateToPage(page, files, create);
}

void MainWindow::openContainer(bool signature)
{
	QString filter = QFileDialog::tr("All Files (*)") + QStringLiteral(";;") + FileDialog::tr("Documents (%1)");
	if(signature)
		filter = filter.arg(QStringLiteral("*.bdoc *.ddoc *.asice *.sce *.asics *.scs *.edoc *.adoc%1")
			.arg(Application::confValue(Application::SiVaUrl).toString().isEmpty() ? QLatin1String() : QLatin1String(" *.pdf")));
	else
		filter = filter.arg(QLatin1String("*.cdoc *.cdoc2"));
	QStringList files = FileDialog::getOpenFileNames(this, tr("Select documents"), {}, filter);
	if(!files.isEmpty())
		openFiles(std::move(files));
}

void MainWindow::resetDigiDoc(std::unique_ptr<DigiDoc> &&doc)
{
	if(digiDoc && digiDoc->isModified())
	{
		auto *dlg = WarningDialog::create(this);
		QString saveTxt;
		if(digiDoc->state() == UnsignedContainer)
		{
			dlg->withText(tr("You've added file(s) to container, but these are not signed yet. Keep the unsigned container or remove it?"))
				->setCancelText(WarningDialog::Remove)
				->addButton(tr("Keep"), QMessageBox::Save);
		}
		else
		{
			dlg->withText(tr("You've changed the open container but have not saved any changes. Save the changes or close without saving?"))
				->setCancelText(tr("Do not save"))
				->addButton(tr("Save"), QMessageBox::Save);
		}
		if(dlg->exec() == QMessageBox::Save)
			digiDoc->save();
	}
	digiDoc = std::move(doc);
}

void MainWindow::selectPage(Pages page)
{
	if(ui->startScreen->currentIndex() == page)
		return;
	auto *btn = page < CryptoIntro ? ui->signature : (page == MyEid ? ui->myEid : ui->crypto);
	btn->setChecked(true);
	ui->startScreen->setCurrentIndex(page);
	ui->warnings->updateWarnings(page);
	adjustDrops();
	updateSelector();
}

void MainWindow::showEvent(QShowEvent * /*event*/)
{
	static bool isShown = false;
	if(isShown)
		return;
	QRect r{{}, QSize{166, 94}};
	r.moveBottomRight(rect().bottomRight() - QPoint{15, 70});
	auto *notification = new FadeInNotification(this, r);
	notification->setFocusPolicy(Qt::NoFocus);
	auto *structureFunds = new QSvgWidget(QStringLiteral(":/images/Struktuurifondid.svg"), notification);
	structureFunds->resize(notification->size());
	notification->start(400ms);
	isShown = true;
}

void MainWindow::showSettings(int page)
{
	if(auto *settings = findChild<SettingsDialog*>())
	{
		settings->showPage(page);
		settings->show();
		return;
	}
	SettingsDialog dlg(page, this);
	connect(&dlg, &SettingsDialog::togglePrinting, ui->signContainerPage, &ContainerPage::togglePrinting);
	dlg.exec();
}

template<typename F>
void MainWindow::sign(F &&sign)
{
	if(!CheckConnection().check())
	{
		FadeInNotification::error(ui->topBar, tr("Check internet connection"));
		return;
	}

	QString role, city, state, country, zip;
	if(RoleAddressDialog(this).get(city, country, state, zip, role) == QDialog::Rejected)
		return;

	WaitDialogHolder waitDialog(this, tr("Signing"));
	if(digiDoc->isPDF())
	{
		QString wrappedFile = digiDoc->fileName();
		if(!wrap(wrappedFile, true))
			return;

		if(!sign(city, state, zip, country, role))
		{
			digiDoc.reset();
			openFiles({std::move(wrappedFile)});
			return;
		}
	}
	else if(!sign(city, state, zip, country, role))
		return;

	if(!digiDoc->save())
		return;

	ui->signContainerPage->transition(digiDoc.get());

	FadeInNotification::success(ui->topBar, tr("The container has been successfully signed!"));
	adjustDrops();
}

void MainWindow::removeSignature(int index)
{
	if(!digiDoc)
		return;
	WaitDialogHolder waitDialog(this, tr("Removing signature"));
	digiDoc->removeSignature(unsigned(index));
	digiDoc->save();
	ui->signContainerPage->transition(digiDoc.get());
	adjustDrops();
}

bool MainWindow::wrap(const QString& wrappedFile, bool pdf)
{
	QString filename = FileDialog::createNewFileName(wrappedFile, true, this);
	if(filename.isNull())
		return false;

	auto signatureContainer = std::make_unique<DigiDoc>(this);
	signatureContainer->create(filename);

	// If pdf, add whole file to signature container; otherwise content only
	if(pdf)
		signatureContainer->documentModel()->addFile(wrappedFile);
	else
		signatureContainer->documentModel()->addTempFiles(cryptoDoc->documentModel()->tempFiles());

	cryptoDoc.reset();
	resetDigiDoc(std::move(signatureContainer));

	ui->signContainerPage->transition(digiDoc.get());
	selectPage(SignDetails);

	return true;
}

bool MainWindow::wrapContainer(bool signing)
{
	return WarningDialog::create(this)
		->withTitle(signing ? tr("Files can not be added to the signed container") : tr("Files can not be added to the cryptocontainer"))
		->withText(signing ?
			tr("The system will create a new container which shall contain the signed document and the files you wish to add.") :
			tr("The system will create a new container which shall contain the cypto-document and the files you wish to add."))
		->setCancelText(WarningDialog::Cancel)
		->addButton(tr("Continue"), QMessageBox::Ok)
		->exec() == QMessageBox::Ok;
}

void MainWindow::updateMyEID(const TokenData &t)
{
	updateSelector();
	ui->myEid->invalidIcon(false);
	ui->myEid->warningIcon(false);
	ui->warnings->closeWarnings(MyEid);
	SslCertificate cert(t.cert());
	auto type = cert.type();
	ui->infoStack->setHidden(type == SslCertificate::UnknownType);
	ui->accordion->setHidden(type == SslCertificate::UnknownType);
	ui->noReaderInfo->setVisible(type == SslCertificate::UnknownType);

	auto setText = [this](const char *text) {
		ui->noReaderInfoText->setProperty("currenttext", text);
		ui->noReaderInfoText->setText(tr(text));
	};
	if(!t.isNull())
	{
		setText(QT_TR_NOOP("The card in the card reader is not an Estonian ID-card"));
		if(ui->cardInfo->token().card() != t.card())
			ui->accordion->clear();
		if(type & SslCertificate::TempelType)
		{
			ui->infoStack->update(cert);
			ui->accordion->updateInfo(cert);
		}
	}
	else
	{
		ui->infoStack->clearData();
		ui->accordion->clear();
		setText(QT_TR_NOOP("Connect the card reader to your computer and insert your ID card into the reader"));
	}
}

void MainWindow::updateMyEid(const QSmartCardData &data)
{
	ui->infoStack->update(data);
	ui->accordion->updateInfo(data);
	ui->myEid->warningIcon(false);
	ui->myEid->invalidIcon(false);
	ui->warnings->closeWarnings(MyEid);
	if(data.isNull())
		return;
	bool pin1Blocked = data.retryCount(QSmartCardData::Pin1Type) == 0;
	bool pin2Blocked = data.retryCount(QSmartCardData::Pin2Type) == 0;
	bool pin2Locked = data.pinLocked(QSmartCardData::Pin2Type);
	ui->myEid->warningIcon(
		pin1Blocked ||
		pin2Blocked || pin2Locked ||
		data.retryCount(QSmartCardData::PukType) == 0);
	ui->signContainerPage->cardChanged(data.signCert(), pin2Blocked || pin2Locked);
	ui->cryptoContainerPage->cardChanged(data.authCert(), pin1Blocked);

	if(pin1Blocked)
		ui->warnings->showWarning({WarningType::UnblockPin1Warning, 0,
			[this]{ changePinClicked(QSmartCardData::Pin1Type, QSmartCard::UnblockWithPuk); }});

	if(pin2Locked && pin2Blocked)
		ui->warnings->showWarning({WarningType::ActivatePin2WithPUKWarning, 0,
			[this]{ changePinClicked(QSmartCardData::Pin2Type, QSmartCard::ActivateWithPuk); }});
	else if(pin2Blocked)
		ui->warnings->showWarning({WarningType::UnblockPin2Warning, 0,
			[this]{ changePinClicked(QSmartCardData::Pin2Type, QSmartCard::UnblockWithPuk); }});
	else if(pin2Locked)
		ui->warnings->showWarning({WarningType::ActivatePin2Warning, 0,
			[this]{ changePinClicked(QSmartCardData::Pin2Type, QSmartCard::ActivateWithPin); }});

	const qint64 DAY = 24 * 60 * 60;
	qint64 expiresIn = 106 * DAY;
	for(const QSslCertificate &cert: {data.authCert(), data.signCert()})
	{
		if(!cert.isNull())
			expiresIn = std::min<qint64>(expiresIn, QDateTime::currentDateTime().secsTo(cert.expiryDate().toLocalTime()));
	}
	if(expiresIn <= 0)
	{
		ui->myEid->invalidIcon(true);
		ui->warnings->showWarning({WarningType::CertExpiredError});
	}
	else if(expiresIn <= 105 * DAY)
	{
		ui->myEid->warningIcon(true);
		ui->warnings->showWarning({WarningType::CertExpiryWarning});
	}
}

void MainWindow::updateSelector()
{
	TokenData selected;
	enum Filter: uint8_t {
		Signing,
		Decrypting,
		MyEID,
	} filter = Signing;
	switch(ui->startScreen->currentIndex())
	{
	case SignIntro:
	case SignDetails:
		selected = qApp->signer()->tokensign();
		filter = Signing;
		break;
	case CryptoIntro:
	case CryptoDetails:
		selected = qApp->signer()->tokenauth();
		filter = Decrypting;
		break;
	case MyEid:
	default:
		selected = qApp->signer()->smartcard()->tokenData();
		filter = MyEID;
		break;
	}
	QVector<TokenData> list;
	for(const TokenData &token: qApp->signer()->cache())
	{
		if(token.card() == selected.card())
			continue;
		if(std::any_of(list.cbegin(), list.cend(), [token](const TokenData &item) { return token.card() == item.card(); }))
			continue;
		SslCertificate cert(token.cert());
		if(filter == Signing && !cert.keyUsage().contains(SslCertificate::NonRepudiation))
			continue;
		if(filter == Decrypting && cert.keyUsage().contains(SslCertificate::NonRepudiation))
			continue;
		if(filter == MyEID &&
			!(cert.type() & SslCertificate::EstEidType || cert.type() & SslCertificate::DigiIDType || cert.type() & SslCertificate::TempelType))
			continue;
		list.append(token);
	}
	ui->noCardInfo->setVisible(selected.isNull());
	ui->selector->setHidden(list.isEmpty());
	ui->selector->setChecked(false);
	ui->cardInfo->setVisible(ui->noCardInfo->isHidden());
	ui->cardInfo->setCursor(ui->selector->isVisible() ? Qt::PointingHandCursor : Qt::ArrowCursor);
	ui->cardInfo->update(selected, list.size() > 1);
	if (!QPCSC::instance().serviceRunning())
		ui->noCardInfo->update(NoCardInfo::NoPCSC);
	else if(QPCSC::instance().readers().isEmpty())
		ui->noCardInfo->update(NoCardInfo::NoReader);
	else
		ui->noCardInfo->update(NoCardInfo::NoCard);
	disconnect(ui->selector, &QToolButton::toggled, this, nullptr);
	if(list.isEmpty())
		return;
	connect(ui->selector, &QToolButton::toggled, this, [this, list](bool show) {
		if(show)
		{
			auto *cardPopup = new CardPopup(list, this);
			connect(cardPopup, &CardPopup::activated, qApp->signer(), &QSigner::selectCard);
			connect(cardPopup, &CardPopup::activated, this, [this] { ui->selector->setChecked(false); });
			cardPopup->show();
		}
		else if(auto *cardPopup = findChild<CardPopup*>())
			cardPopup->deleteLater();
	});
}
