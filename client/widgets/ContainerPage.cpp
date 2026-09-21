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

#include "Application.h"
#include "CryptoDoc.h"
#include "DigiDoc.h"
#include "PrintSheet.h"
#include "QCryptoBackend.h"
#include "Settings.h"
#include "SslCertificate.h"
#include "TokenData.h"
#include "dialogs/AddRecipients.h"
#include "dialogs/FileDialog.h"
#include "dialogs/PasswordDialog.h"
#include "dialogs/WarningDialog.h"
#include "widgets/AddressItem.h"
#include "widgets/MainAction.h"
#include "widgets/SignatureItem.h"
#include "widgets/WarningItem.h"

#include <QDir>
#include <QFileInfo>
#include <QFontMetrics>
#include <QMessageBox>
#include <QTabBar>

#include <QtCore/QUrlQuery>
#include <QtGui/QDesktopServices>

#include <QtPrintSupport/QPrintPreviewDialog>
#include <QtPrintSupport/QPrinterInfo>

using namespace ria::qdigidoc4;

ContainerPage::ContainerPage(QWidget *parent)
: QWidget(parent)
, ui(new Ui::ContainerPage)
{
	ui->setupUi( this );
	mainAction = new MainAction(this);
	ui->leftPane->init(fileName);
	ui->containerFile->installEventFilter(this);
	ui->summary->hide();
	ui->extend->hide();

	auto connectCode = [this](QAbstractButton *btn, int code) {
		connect(btn, &QAbstractButton::clicked, this, [this,code] { emit action(code); });
	};

	connect(mainAction, &MainAction::action, this, &ContainerPage::action);
	connect(ui->cancel, &QPushButton::clicked, this, [this] {
		window()->setWindowFilePath({});
		window()->setWindowTitle(tr("DigiDoc4 Client"));
		emit action(Actions::ContainerCancel);
	});
	connectCode(ui->convert, Actions::ContainerConvert);
	connect(ui->leftPane, &FileList::addFiles, this, &ContainerPage::addFiles);
	connect(ui->leftPane, &ItemList::addItem, this, [this](int code) { emit action(code); });
	connect(ui->rightPane, &ItemList::addItem, this, [this](int code) { emit action(code); });
	ui->encryptMethodArea->hide();
	ui->encryptMethod->setExpanding(true);
	ui->encryptMethod->setDrawBase(false);
	ui->encryptMethod->setIconSize({16, 16});
	ui->encryptMethod->addTab(tr("Encrypt for recipients"));
	ui->encryptMethod->addTab(tr("Encrypt with password"));
	connect(ui->encryptMethod, &QTabBar::currentChanged, this, [this](int index) {
		for(int i = 0; i < ui->encryptMethod->count(); ++i)
			ui->encryptMethod->setTabIcon(i, i == index ?
				QIcon(QStringLiteral(":/images/icon_check_white.svg")) : QIcon());
		ui->rightPaneStack->setCurrentIndex(index);
	});
	ui->encryptMethod->setTabIcon(0, QIcon(QStringLiteral(":/images/icon_check_white.svg")));
	connect(ui->email, &QAbstractButton::clicked, this, [this] {
		if(!QFileInfo::exists(fileName))
			return;
		QUrlQuery q;
		q.addQueryItem(QStringLiteral("subject"), QFileInfo(fileName).fileName());
		q.addQueryItem(QStringLiteral("attachment"), QFileInfo(fileName).absoluteFilePath());
		QUrl url;
		url.setScheme(QStringLiteral("mailto"));
		url.setQuery(q);
		QDesktopServices::openUrl(url);
	});
	connect(ui->containerFile, &QLabel::linkActivated, this, [this]{
		if(!QFileInfo::exists(fileName))
			return;
		QUrl url = QUrl::fromLocalFile( fileName );
		url.setScheme(QStringLiteral("browse"));
		QDesktopServices::openUrl(url);
	});
}

ContainerPage::~ContainerPage()
{
	delete ui;
}

void ContainerPage::clear(int code)
{
	ui->leftPane->clear();
	ui->rightPane->clear();
	emit action(code);
}

void ContainerPage::elideFileName()
{
	ui->containerFile->setText(QStringLiteral("<a href='#browse-Container' style='color:#215081'>%1</a>")
		.arg(ui->containerFile->fontMetrics().elidedText(
			FileDialog::normalized(fileName).toHtmlEscaped(), Qt::ElideMiddle, ui->containerFile->width())));
}

bool ContainerPage::eventFilter(QObject *o, QEvent *e)
{
	switch(e->type())
	{
	case QEvent::Resize:
	case QEvent::LanguageChange:
		if(o == ui->containerFile)
			elideFileName();
		break;
	default: break;
	}
	return QWidget::eventFilter(o, e);
}

void ContainerPage::changeEvent(QEvent* event)
{
	if (event->type() == QEvent::LanguageChange)
	{
		ui->retranslateUi(this);
		translateLabels();
	}

	QWidget::changeEvent(event);
}

void ContainerPage::decrypt(CryptoDoc *container, const libcdoc::Lock &lock, const QByteArray &secret, const TokenData &token) {
	WaitDialogHolder waitDialog(this, tr("Decrypting"));
	if (!container->decrypt(lock, secret, token))
		return;
	transition(container);
	emit action(DecryptContainerSuccess);
}

template<class C>
bool ContainerPage::deleteConfirm(C *c, int index)
{
	if(c->documentModel()->rowCount() > 1)
	{
		ui->leftPane->removeItem(index);
		return true;
	}
	auto *dlg = WarningDialog::create(this)
		->withTitle(tr("You are about to delete the last file in the container"))
		->withText(tr("It is removed along with the container."))
		->setCancelText(WarningDialog::Cancel)
		->resetCancelStyle(false)
		->addButton(WarningDialog::Remove, QMessageBox::Ok, true);
	if (dlg->exec() != QMessageBox::Ok)
		return false;
	window()->setWindowFilePath({});
	window()->setWindowTitle(QCoreApplication::translate("MainWindow", "DigiDoc4 Client"));
	if(QFile::exists(c->fileName()))
		QFile::remove(c->fileName());
	emit action(ContainerClose);
	return false;
}

void ContainerPage::encrypt(CryptoDoc *container)
{
	QString label;
	QByteArray secret;
	if(isPasswordEncryption()) {
		PasswordDialog p(PasswordDialog::Mode::ENCRYPT, this);
		if(!p.exec())
			return;
		label = p.label();
		secret = p.secret();
	}

	WaitDialogHolder waitDialog(this, tr("Encrypting"));
	if(!container->encrypt(container->fileName(), label, secret))
		return;
	transition(container);
	emit action(EncryptContainerSuccess);
}

void ContainerPage::setHeader(const QString &file)
{
	fileName = QDir::toNativeSeparators (file);
	window()->setWindowFilePath(fileName);
	window()->setWindowTitle(file.isEmpty() ? tr("DigiDoc4 Client") : QFileInfo(file).fileName());
	elideFileName();
}

bool ContainerPage::isPasswordEncryption() const
{
	return !ui->encryptMethodArea->isHidden() && ui->encryptMethod->currentIndex() == 1;
}

void ContainerPage::transition(CryptoDoc *container)
{
	disconnect(ui->leftPane, &ItemList::removed, container, nullptr);
	connect(ui->leftPane, &ItemList::removed, container, [this, container](int index) {
		deleteConfirm(container, index);
	});
	disconnect(ui->rightPane, &ItemList::add, container, nullptr);
	connect(ui->rightPane, &ItemList::add, container, [this, container] {
		AddRecipients dlg(ui->rightPane, this);
		if(!dlg.exec() || !dlg.isUpdated())
			return;
		container->clearKeys();
		ui->rightPane->clear();
		for(auto &key: dlg.keys())
		{
			container->addEncryptionKey(key);
			ui->rightPane->addWidget(new AddressItem(key, AddressItem::Icon, ui->rightPane));
		}
		mainAction->setEnabled(isEncryptEnabled(container));
	});
	disconnect(ui->rightPane, &ItemList::removed, container, nullptr);
	connect(ui->rightPane, &ItemList::removed, container, [this, container](int index) {
		container->removeKey(index);
		ui->rightPane->removeItem(index);
		mainAction->setEnabled(isEncryptEnabled(container));
	});
	disconnect(ui->changeLocation, &QPushButton::clicked, container, nullptr);
	connect(ui->changeLocation, &QPushButton::clicked, container, [container, this] {
		QString to = FileDialog::getSaveFileName(this, FileDialog::tr("Move file"), container->fileName());
		if(!to.isNull() && container->move(to))
			setHeader(to);
	});
	disconnect(ui->saveAs, &QPushButton::clicked, container, nullptr);
	connect(ui->saveAs, &QPushButton::clicked, container, [container, this] {
		if(QString target = FileDialog::getSaveFileName(this, FileDialog::tr("Save file"), container->fileName()); !target.isEmpty())
			container->saveCopy(target);
	});
	disconnect(container, &CryptoDoc::destroyed, this, nullptr);
	connect(container, &CryptoDoc::destroyed, this, [this] {
		clear(ContainerClearWarning);
	});
	disconnect(ui->encryptMethod, &QTabBar::currentChanged, container, nullptr);
	connect(ui->encryptMethod, &QTabBar::currentChanged, container, [this, container] {
		mainAction->setEnabled(isEncryptEnabled(container));
	});
	disconnect(mainAction, &MainAction::action, container, nullptr);
	connect(mainAction, &MainAction::action, container, [container, this](int action) {
		if(action == EncryptContainer)
			encrypt(container);
	});

	clear(ContainerClearWarning);
	ui->encryptMethod->setCurrentIndex(0);
	setHeader(container->fileName());
	ui->leftPane->init(fileName, QT_TRANSLATE_NOOP("ItemList", "Encrypted files"));
	ui->rightPane->init(ItemList::ItemAddress, QT_TRANSLATE_NOOP("ItemList", "Recipients"));
	bool hasUnsupported = false;
	for (const auto &key : container->keys()) {
		hasUnsupported = hasUnsupported || (key.rcpt_cert.isNull() && !key.lock.isValid());
		auto *addr = new AddressItem(key, AddressItem::Icon, ui->rightPane);
		ui->rightPane->addWidget(addr);
		connect(addr, &AddressItem::decrypt, container, [container, key, this](const TokenData &token) {
			if (key.lock.type != libcdoc::Lock::Type::PASSWORD) {
				decrypt(container, key.lock, {}, token);
				return;
			}
			PasswordDialog p(PasswordDialog::Mode::DECRYPT, this);
			auto params = libcdoc::Lock::parseLabel(key.lock.label);
			p.setLabel(QString::fromStdString(params.contains("label") ? params["label"] : key.lock.label));
			if (!p.exec())
				return;
			decrypt(container, key.lock, p.secret(), token);
		});
	}
	if(hasUnsupported)
		emit warning({WarningText::UnsupportedCDocWarning});
	ui->leftPane->setModel(container->documentModel());
	updatePanes(container->state(), container);
}

void ContainerPage::transition(DigiDoc* container)
{
	using enum WarningText::WarningType;
	disconnect(ui->leftPane, &ItemList::removed, container, nullptr);
	connect(ui->leftPane, &ItemList::removed, container, [this, container](int index) {
		if(deleteConfirm(container, index))
			transition(container);
	});
	disconnect(ui->summary, &QAbstractButton::clicked, container, nullptr);
	connect(ui->summary, &QAbstractButton::clicked, container, [this,container] {
#ifdef Q_OS_WIN
		if( QPrinterInfo::availablePrinterNames().isEmpty() )
		{
			WarningDialog::create(this)
				->withText(tr("In order to view Validity Confirmation Sheet there has to be at least one printer installed!"))
				->open();
			return;
		}
#endif
		auto *dialog = new QPrintPreviewDialog( this );
		dialog->setAttribute(Qt::WA_DeleteOnClose);
		dialog->printer()->setPageSize( QPageSize( QPageSize::A4 ) );
		dialog->printer()->setPageOrientation( QPageLayout::Portrait );
		dialog->setMinimumHeight( 700 );
		connect(dialog, &QPrintPreviewDialog::paintRequested, container, [container](QPrinter *printer) {
			PrintSheet(container, printer);
		});
		dialog->open();
	});
	disconnect(ui->changeLocation, &QPushButton::clicked, container, nullptr);
	connect(ui->changeLocation, &QPushButton::clicked, container, [container, this] {
		QString to = FileDialog::getSaveFileName(this, FileDialog::tr("Move file"), container->fileName());
		if(!to.isNull() && container->move(to))
			setHeader(to);
	});
	disconnect(ui->extend, &QPushButton::clicked, container, nullptr);
	connect(ui->extend, &QPushButton::clicked, container, [container, this] {
		auto *d = WarningDialog::create(this)
			->withTitle(tr("Extend signatures"))
			->withText(tr("All signatures in the container will be extended to LTA format."))
			->addButton(tr("Extend"), QMessageBox::Yes);
		if(d->exec() != QMessageBox::Yes)
			return;
		WaitDialogHolder waitDialog(this, tr("Extend signatures"));
		if(container->extend())
			transition(container);
	});
	disconnect(ui->save, &QPushButton::clicked, container, nullptr);
	connect(ui->save, &QPushButton::clicked, container, [container, this] {
		if(container->save())
			updatePanes(container->state(), nullptr);
	});
	disconnect(ui->saveAs, &QPushButton::clicked, container, nullptr);
	connect(ui->saveAs, &QPushButton::clicked, container, [container, this] {
		if(QString target = FileDialog::getSaveFileName(this, FileDialog::tr("Save file"), container->fileName()); !target.isEmpty())
			container->saveAs(target);
	});
	disconnect(ui->rightPane, &ItemList::removed, container, nullptr);
	connect(ui->rightPane, &ItemList::removed, container, [this, container](int index) {
		WaitDialogHolder waitDialog(this, tr("Removing signature"));
		if(!container->removeSignature(unsigned(index)))
			return;
		ui->rightPane->removeItem(index);
		if(container->save())
			setHeader(container->fileName());
		updatePanes(container->state(), nullptr);
	});
	disconnect(container, &DigiDoc::destroyed, this, nullptr);
	connect(container, &DigiDoc::destroyed, this, [this] {
		clear(ContainerClearWarning);
	});

	clear(ContainerClearWarning);
	std::map<WarningText::WarningType, int> errors;
	setHeader(container->fileName());
	ui->leftPane->init(fileName, QT_TRANSLATE_NOOP("ItemList", "Container files"));

	if(!container->timestamps().isEmpty())
	{
		ui->rightPane->addHeader(QT_TRANSLATE_NOOP("ItemList", "Container timestamps"));

		for(const DigiDocSignature &c: container->timestamps())
		{
			auto *item = new SignatureItem(c, ui->rightPane);
			if(c.isInvalid())
				++errors[item->getError()];
			ui->rightPane->addHeaderWidget(item);
		}
	}

	for(const DigiDocSignature &c: container->signatures())
	{
		auto *item = new SignatureItem(c, ui->rightPane);
		if(c.isInvalid())
			++errors[item->getError()];
		ui->rightPane->addWidget(item);
	}

	for(const auto &[key, value]: errors)
		emit warning({key, value});
	if(container->fileName().endsWith(QStringLiteral("ddoc"), Qt::CaseInsensitive))
		emit warning({UnsupportedDDocWarning});
	if(container->isAsicS())
		emit warning({UnsupportedAsicSWarning});
	if(container->isCades())
		emit warning({UnsupportedAsicCadesWarning});

	isSupported = container->isSupported() || container->isPDF();

	for (auto i = 0, count = container->documentModel()->rowCount(); i < count; i++)
	{
		if(container->documentModel()->fileSize(i) == 0)
		{
			emit warning({EmptyFileWarning});
			isSupported = false;
			break;
		}
	}

	ui->leftPane->setModel(container->documentModel());
	updatePanes(container->state(), nullptr);
}

bool ContainerPage::isEncryptEnabled(CryptoDoc *container) const
{
	return isPasswordEncryption() || (container && !container->keys().empty());
}

void ContainerPage::updatePanes(ria::qdigidoc4::ContainerState state, CryptoDoc *crypto_container)
{
	ui->leftPane->stateChange(state);
	ui->rightPane->stateChange(state);
	ui->encryptMethodArea->setVisible(state == UnencryptedContainer && crypto_container && crypto_container->supportsSymmetricKeys());
	if(ui->encryptMethodArea->isHidden())
		ui->encryptMethod->setCurrentIndex(0);
	ui->save->setVisible(state == UnsignedContainer);
	ui->rightPaneArea->setHidden(state == UnsignedContainer);
	auto setButtonsVisible = [](std::initializer_list<QWidget*> buttons, bool visible) {
		for(QWidget *button: buttons) button->setVisible(visible);
	};

	switch(state)
	{
	case UnsignedContainer:
		cancelText = QT_TR_NOOP("Cancel");

		ui->changeLocation->show();
		ui->rightPane->clear();
		setButtonsVisible({ ui->saveAs, ui->email, ui->summary, ui->extend }, false);
		break;
	case UnsignedSavedContainer:
		cancelText = QT_TR_NOOP("Start");

		ui->changeLocation->show();
		ui->rightPane->init(ItemList::ItemSignature, QT_TRANSLATE_NOOP("ItemList", "Container is not signed"));
		ui->summary->setVisible(Settings::SHOW_PRINT_SUMMARY);
		setButtonsVisible({ ui->saveAs, ui->email }, true);
		ui->extend->hide();
		break;
	case SignedContainer:
		cancelText = QT_TR_NOOP("Start");

		ui->changeLocation->hide();
		ui->rightPane->init(ItemList::ItemSignature, QT_TRANSLATE_NOOP("ItemList", "Container signatures"));
		ui->summary->setVisible(Settings::SHOW_PRINT_SUMMARY);
		setButtonsVisible({ ui->saveAs, ui->email, ui->extend }, true);
		break;
	case UnencryptedContainer:
		cancelText = QT_TR_NOOP("Start");
		convertText = QT_TR_NOOP("Sign");
		setButtonsVisible({ ui->changeLocation, ui->convert }, true);
		setButtonsVisible({ ui->saveAs, ui->email, ui->extend }, false);
		break;
	case EncryptedContainer:
		cancelText = QT_TR_NOOP("Start");
		convertText = QT_TR_NOOP("Sign");
		setButtonsVisible({ ui->changeLocation, ui->convert, ui->extend }, false);
		setButtonsVisible({ ui->saveAs, ui->email }, true);
		break;
	default:
		// Uninitialized cannot be shown on container page
		break;
	}

	switch(state)
	{
	case UnsignedContainer:
	case UnsignedSavedContainer:
	case SignedContainer:
		if(isSupported)
		{
			mainAction->showAction(SignatureAdd);
			mainAction->setEnabled(true);
		}
		else
			mainAction->hide();
		break;
	case UnencryptedContainer:
		mainAction->showAction(EncryptContainer);
		mainAction->setEnabled(isEncryptEnabled(crypto_container));
		break;
	default:
		mainAction->hide();
		break;
	}
	ui->mainActionSpacer->changeSize(mainAction->isHidden() ? 1 : 198, 20, QSizePolicy::Fixed);
	ui->navigationArea->layout()->invalidate();

	translateLabels();
}

void ContainerPage::togglePrinting(bool enable)
{
	ui->summary->setVisible(enable);
}

void ContainerPage::translateLabels()
{
	ui->cancel->setText(tr(cancelText));
	ui->convert->setText(tr(convertText));
}
