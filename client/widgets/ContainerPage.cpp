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

#include "CryptoDoc.h"
#include "DigiDoc.h"
#include "PrintSheet.h"
#include "Settings.h"
#include "SslCertificate.h"
#include "dialogs/AddRecipients.h"
#include "dialogs/FileDialog.h"
#include "dialogs/MobileDialog.h"
#include "dialogs/SmartIDDialog.h"
#include "dialogs/WarningDialog.h"
#include "widgets/AddressItem.h"
#include "widgets/SignatureItem.h"
#include "widgets/WarningItem.h"

#include <QDir>
#include <QFileInfo>
#include <QFontMetrics>
#include <QMessageBox>

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
	ui->leftPane->init(fileName);
	ui->containerFile->installEventFilter(this);
	ui->summary->hide();

	auto connectCode = [this](QAbstractButton *btn, int code) {
		connect(btn, &QAbstractButton::clicked, this, [this,code] { emit action(code); });
	};

	connect(ui->cancel, &QPushButton::clicked, this, [this] {
		window()->setWindowFilePath({});
		window()->setWindowTitle(tr("DigiDoc4 Client"));
		emit action(Actions::ContainerCancel);
	});
	connectCode(ui->convert, Actions::ContainerConvert);
	connectCode(ui->save, Actions::ContainerSave);
	connect(ui->leftPane, &FileList::addFiles, this, &ContainerPage::addFiles);
	connect(ui->leftPane, &ItemList::removed, this, &ContainerPage::fileRemoved);
	connect(ui->leftPane, &ItemList::addItem, this, [this](int code) { emit action(code); });
	connect(ui->rightPane, &ItemList::addItem, this, [this](int code) { emit action(code); });
	connect(ui->rightPane, &ItemList::removed, this, &ContainerPage::removed);
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

void ContainerPage::cardChanged(const SslCertificate &cert, bool isBlocked)
{
	emit ui->rightPane->idChanged(cert);
	isSeal = cert.type() & SslCertificate::TempelType;
	isExpired = !cert.isValid();
	this->isBlocked = isBlocked;
	idCode = cert.personalCode();
	emit certChanged(cert);
}

void ContainerPage::clear(int code)
{
	ui->leftPane->clear();
	ui->rightPane->clear();
	emit action(code);
}

void ContainerPage::clearPopups()
{
	if(mainAction) mainAction->hideDropdown();
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

void ContainerPage::handleAction(int type)
{
	QString code;
	QString info2;
	switch(type)
	{
	case SignatureAdd:
	case SignatureToken:
		code = idCode;
		break;
	case SignatureMobile:
	{
		MobileDialog dlg(this);
		if(dlg.exec() != QDialog::Accepted)
			return;
		code = dlg.idCode();
		info2 = dlg.phoneNo();
		break;
	}
	case SignatureSmartID:
	{
		SmartIDDialog dlg(this);
		if(dlg.exec() != QDialog::Accepted)
			return;
		code = dlg.idCode();
		info2 = dlg.country();
		break;
	}
	default:
		emit action(type, code, info2);
		return;
	}
	if(auto items = ui->rightPane->findChildren<SignatureItem*>();
		std::any_of(items.cbegin(), items.cend(), [code](auto *signatureItem) {
			return signatureItem->isSelfSigned(code);
		}))
	{
		auto *dlg = new WarningDialog(tr("The document has already been signed by you."), this);
		dlg->addButton(tr("Continue signing"), QMessageBox::Ok);
		if(dlg->exec() != QMessageBox::Ok)
			return;
	}
	emit action(type, code, info2);
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

void ContainerPage::setHeader(const QString &file)
{
	fileName = QDir::toNativeSeparators (file);
	window()->setWindowFilePath(fileName);
	window()->setWindowTitle(file.isEmpty() ? tr("DigiDoc4 Client") : QFileInfo(file).fileName());
	elideFileName();
}

void ContainerPage::showMainAction(const QList<Actions> &actions)
{
	if(!mainAction)
	{
		mainAction = std::make_unique<MainAction>(this);
		connect(mainAction.get(), &MainAction::action, this, &ContainerPage::handleAction);
	}
	mainAction->showActions(actions);
	bool isSignCard = actions.contains(SignatureAdd) || actions.contains(SignatureToken);
	bool isSignMobile = !isSignCard && (actions.contains(SignatureMobile) || actions.contains(SignatureSmartID));
	bool isEncrypt = actions.contains(EncryptContainer) && !ui->rightPane->findChildren<AddressItem*>().isEmpty();
	bool isDecrypt = !isBlocked && (actions.contains(DecryptContainer) || actions.contains(DecryptToken));
	mainAction->setButtonEnabled(isSupported && !hasEmptyFile &&
		(isEncrypt || isDecrypt || isSignMobile || (isSignCard && !isBlocked && !isExpired)));
	ui->mainActionSpacer->changeSize(198, 20, QSizePolicy::Fixed);
	ui->navigationArea->layout()->invalidate();
}

void ContainerPage::showSigningButton()
{
	if (!isSupported || hasEmptyFile)
	{
		if(mainAction)
			mainAction->hide();
		ui->mainActionSpacer->changeSize(1, 20, QSizePolicy::Fixed);
		ui->navigationArea->layout()->invalidate();
	}
	else if(idCode.isEmpty())
		showMainAction({ SignatureMobile, SignatureSmartID });
	else if(isSeal)
		showMainAction({ SignatureToken, SignatureMobile, SignatureSmartID });
	else
		showMainAction({ SignatureAdd, SignatureMobile, SignatureSmartID });
}

void ContainerPage::transition(CryptoDoc *container, const QSslCertificate &cert)
{
	disconnect(ui->rightPane, &ItemList::add, container, nullptr);
	connect(ui->rightPane, &ItemList::add, container, [this, container] {
		AddRecipients dlg(ui->rightPane, this);
		if(!dlg.exec() || !dlg.isUpdated())
			return;
		for(auto i = container->keys().size() - 1; i >= 0; i--)
			container->removeKey(i);
		ui->rightPane->clear();
		for(auto &key: dlg.keys())
		{
			container->addKey(key);
			ui->rightPane->addWidget(new AddressItem(std::move(key), AddressItem::Icon, ui->rightPane));
		}
		showMainAction({ EncryptContainer });
	});
	disconnect(this, &ContainerPage::removed, container, nullptr);
	connect(this, &ContainerPage::removed, container, [this, container](int index) {
		container->removeKey(index);
		ui->rightPane->removeItem(index);
		showMainAction({ EncryptContainer });
	});
	disconnect(this, &ContainerPage::certChanged, container, nullptr);
	connect(this, &ContainerPage::certChanged, container, [this, container](const SslCertificate &cert) {
		isSupported = container->state() & UnencryptedContainer || container->canDecrypt(cert);
		if(container->state() & EncryptedContainer)
			updateDecryptionButton();
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
		clear(ClearCryptoWarning);
	});

	clear(ClearCryptoWarning);
	isSupported = container->state() & UnencryptedContainer || container->canDecrypt(cert);
	setHeader(container->fileName());
	ui->leftPane->init(fileName, QT_TRANSLATE_NOOP("ItemList", "Encrypted files"));
	ui->rightPane->init(ItemAddress, QT_TRANSLATE_NOOP("ItemList", "Recipients"));
	bool hasUnsupported = false;
	for(CKey &key: container->keys())
	{
		hasUnsupported = std::max(hasUnsupported, key.unsupported);
		ui->rightPane->addWidget(new AddressItem(std::move(key), AddressItem::Icon, ui->rightPane));
	}
	if(hasUnsupported)
		emit warning({UnsupportedCDocWarning});
	ui->leftPane->setModel(container->documentModel());
	updatePanes(container->state());
}

void ContainerPage::transition(DigiDoc* container)
{
	disconnect(this, &ContainerPage::certChanged, container, nullptr);
	connect(this, &ContainerPage::certChanged, container, [this](const SslCertificate &) {
		showSigningButton();
	});
	disconnect(ui->summary, &QAbstractButton::clicked, container, nullptr);
	connect(ui->summary, &QAbstractButton::clicked, container, [this,container] {
#ifdef Q_OS_WIN
		if( QPrinterInfo::availablePrinterNames().isEmpty() )
		{
			WarningDialog::show(this,
				tr("In order to view Validity Confirmation Sheet there has to be at least one printer installed!"));
			return;
		}
#endif
		auto *dialog = new QPrintPreviewDialog( this );
		dialog->printer()->setPageSize( QPageSize( QPageSize::A4 ) );
		dialog->printer()->setPageOrientation( QPageLayout::Portrait );
		dialog->setMinimumHeight( 700 );
		connect(dialog, &QPrintPreviewDialog::paintRequested, container, [container](QPrinter *printer) {
			PrintSheet(container, printer);
		});
		dialog->exec();
		dialog->deleteLater();
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
			container->saveAs(target);
	});
	disconnect(container, &DigiDoc::destroyed, this, nullptr);
	connect(container, &DigiDoc::destroyed, this, [this] {
		clear(ClearSignatureWarning);
	});

	clear(ClearSignatureWarning);
	std::map<ria::qdigidoc4::WarningType, int> errors;
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

	hasEmptyFile = false;
	for (auto i = 0; i < container->documentModel()->rowCount(); i++)
	{
		if(container->documentModel()->fileSize(i) == 0)
		{
			emit warning({EmptyFileWarning});
			hasEmptyFile = true;
		}
	}

	isSupported = container->isSupported() || container->isPDF();
	showSigningButton();

	ui->leftPane->setModel(container->documentModel());
	updatePanes(container->state());
}

void ContainerPage::updateDecryptionButton()
{
	showMainAction({ isSeal ? DecryptToken : DecryptContainer });
}

void ContainerPage::updatePanes(ContainerState state)
{
	ui->leftPane->stateChange(state);
	ui->rightPane->stateChange(state);
	ui->save->setVisible(state == UnsignedContainer);
	ui->rightPane->setHidden(state == UnsignedContainer);
	auto setButtonsVisible = [](const QVector<QWidget*> &buttons, bool visible) {
		for(QWidget *button: buttons) button->setVisible(visible);
	};

	switch( state )
	{
	case UnsignedContainer:
		cancelText = QT_TR_NOOP("Cancel");

		ui->changeLocation->show();
		ui->rightPane->clear();
		showSigningButton();
		setButtonsVisible({ ui->saveAs, ui->email, ui->summary }, false);
		break;
	case UnsignedSavedContainer:
		cancelText = QT_TR_NOOP("Start");

		ui->changeLocation->show();
		ui->rightPane->init(ItemSignature, QT_TRANSLATE_NOOP("ItemList", "Container is not signed"));
		ui->summary->setVisible(Settings::SHOW_PRINT_SUMMARY);
		setButtonsVisible({ ui->saveAs, ui->email }, true);
		break;
	case SignedContainer:
		cancelText = QT_TR_NOOP("Start");

		ui->changeLocation->hide();
		ui->rightPane->init(ItemSignature, QT_TRANSLATE_NOOP("ItemList", "Container signatures"));
		ui->summary->setVisible(Settings::SHOW_PRINT_SUMMARY);
		setButtonsVisible({ ui->saveAs, ui->email }, true);
		break;
	case UnencryptedContainer:
		cancelText = QT_TR_NOOP("Start");
		convertText = QT_TR_NOOP("Sign");
		showMainAction({ EncryptContainer });
		setButtonsVisible({ ui->changeLocation, ui->convert }, true);
		setButtonsVisible({ ui->saveAs, ui->email }, false);
		break;
	case EncryptedContainer:
		cancelText = QT_TR_NOOP("Start");
		convertText = QT_TR_NOOP("Sign");
		updateDecryptionButton();
		setButtonsVisible({ ui->changeLocation, ui->convert }, false);
		setButtonsVisible({ ui->saveAs, ui->email }, true);
		break;
	default:
		// Uninitialized cannot be shown on container page
		break;
	}

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
