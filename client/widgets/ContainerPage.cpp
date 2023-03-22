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
#include "Settings.h"
#include "Styles.h"
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

using namespace ria::qdigidoc4;

ContainerPage::ContainerPage(QWidget *parent)
: QWidget(parent)
, ui(new Ui::ContainerPage)
{
	ui->setupUi( this );
	ui->leftPane->init(fileName);

	ui->container->setFont(Styles::font(Styles::Regular, 14));
	ui->containerFile->setFont(Styles::font(Styles::Regular, 14));

	ui->changeLocation->setIcons(QStringLiteral("/images/icon_Edit.svg"),
		QStringLiteral("/images/icon_Edit_hover.svg"), QStringLiteral("/images/icon_Edit_pressed.svg"), 18, 18);
	ui->changeLocation->init( LabelButton::BoxedDeepCeruleanWithCuriousBlue, tr("CHANGE"), Actions::ContainerLocation );
	ui->containerFile->installEventFilter(this);
	ui->cancel->init( LabelButton::BoxedMojo, tr("CANCEL"), Actions::ContainerCancel );
	ui->convert->init( LabelButton::BoxedDeepCerulean, tr("ENCRYPT"), Actions::ContainerConvert );
	ui->saveAs->init( LabelButton::BoxedDeepCerulean, tr("SAVE AS"), Actions::ContainerSaveAs );
	ui->email->init( LabelButton::BoxedDeepCerulean, tr("SEND WITH E-MAIL"), Actions::ContainerEmail );
	ui->summary->init( LabelButton::BoxedDeepCerulean, tr("PRINT SUMMARY"), Actions::ContainerSummary );
	ui->save->init( LabelButton::BoxedDeepCerulean, tr("SAVE WITHOUT SIGNING"), Actions::ContainerSave );

	mobileCode = Settings::MOBILEID_CODE;

	connect(this, &ContainerPage::moved,this, &ContainerPage::setHeader);
	connect(ui->changeLocation, &LabelButton::clicked, this, &ContainerPage::forward);
	connect(ui->cancel, &LabelButton::clicked, this, &ContainerPage::forward);
	connect(ui->save, &LabelButton::clicked, this, &ContainerPage::forward);
	connect(ui->leftPane, &FileList::addFiles, this, &ContainerPage::addFiles);
	connect(ui->leftPane, &ItemList::removed, this, &ContainerPage::fileRemoved);
	connect(ui->leftPane, &ItemList::addItem, this, &ContainerPage::forward);
	connect(ui->rightPane, &ItemList::addItem, this, &ContainerPage::forward);
	connect(ui->rightPane, &ItemList::addressSearch, this, &ContainerPage::addressSearch);
	connect(ui->rightPane, &ItemList::removed, this, &ContainerPage::removed);
	connect(ui->email, &LabelButton::clicked, this, &ContainerPage::forward);
	connect(ui->summary, &LabelButton::clicked, this, &ContainerPage::forward);
	connect(ui->saveAs, &LabelButton::clicked, this, &ContainerPage::forward);
	connect(ui->convert, &LabelButton::clicked, this, &ContainerPage::forward);
	connect(ui->containerFile, &QLabel::linkActivated, this, [this](const QString &link)
		{ emit action(Actions::ContainerNavigate, link); });

	ui->summary->setVisible(Settings::SHOW_PRINT_SUMMARY);
}

ContainerPage::~ContainerPage()
{
	delete ui;
}

void ContainerPage::addressSearch()
{
	AddRecipients dlg(ui->rightPane, this);
	if(dlg.exec() && dlg.isUpdated())
		emit keysSelected(dlg.keys());
}

void ContainerPage::cardChanged(const SslCertificate &cert, bool isBlocked)
{
	emit ui->rightPane->idChanged(cert);
	isSeal = cert.type() & SslCertificate::TempelType;
	isExpired = !cert.isValid();
	this->isBlocked = isBlocked;
	cardChanged(cert.personalCode());
}

void ContainerPage::cardChanged(const QString &idCode)
{
	cardInReader = idCode;
	if(ui->leftPane->getState() & SignatureContainers)
		showSigningButton();
	else if(ui->leftPane->getState() & EncryptedContainer)
		updateDecryptionButton();
}

bool ContainerPage::checkAction(int code, const QString& selectedCard, const QString& selectedMobile)
{
	switch(code)
	{
	case SignatureAdd:
	case SignatureToken:
	case SignatureMobile:
	case SignatureSmartID:
		if(ui->rightPane->hasItem(
			[selectedCard, selectedMobile, code](Item* const item) -> bool
			{
				auto *signatureItem = qobject_cast<SignatureItem* const>(item);
				return signatureItem && signatureItem->isSelfSigned(selectedCard, (code == SignatureMobile) ? selectedMobile: QString());
			}
		))
		{
			auto *dlg = new WarningDialog(tr("The document has already been signed by you."), this);
			dlg->addButton(tr("CONTINUE SIGNING"), SignatureAdd);
			return dlg->exec() == SignatureAdd;
		}
		break;
	default: break;
	}
	return true;
}

void ContainerPage::clear()
{
	ui->leftPane->clear();
	ui->rightPane->clear();
	isSupported = false;
}

void ContainerPage::clearPopups()
{
	if(mainAction) mainAction->hideDropdown();
}

void ContainerPage::elideFileName()
{
	ui->containerFile->setText(QStringLiteral("<a href='#browse-Container' style='color:rgb(53, 55, 57)'>%1</a>")
		.arg(ui->containerFile->fontMetrics().elidedText(FileDialog::normalized(fileName).toHtmlEscaped(), Qt::ElideMiddle, ui->containerFile->width())));
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

void ContainerPage::forward(int code)
{
	switch (code)
	{
	case SignatureMobile:
	{
		if(MobileDialog dlg(this); dlg.exec() == QDialog::Accepted)
		{
			if(checkAction(SignatureMobile, dlg.idCode(), dlg.phoneNo()))
				emit action(SignatureMobile, dlg.idCode(), dlg.phoneNo());
		}

		if(QString newCode = Settings::MOBILEID_CODE; newCode != mobileCode)
		{
			mobileCode = newCode;
			cardChanged(cardInReader);
		}
		break;
	}
	case SignatureSmartID:
	{
		if(SmartIDDialog dlg(this); dlg.exec() == QDialog::Accepted)
		{
			if(checkAction(SignatureMobile, dlg.idCode(), {}))
				emit action(SignatureSmartID, dlg.country(), dlg.idCode());
		}

		if(QString newCode = Settings::SMARTID_CODE; newCode != mobileCode)
		{
			mobileCode = newCode;
			cardChanged(cardInReader);
		}
		break;
	}
	case ContainerCancel:
		window()->setWindowFilePath({});
		window()->setWindowTitle(tr("DigiDoc4 Client"));
		emit action(code);
		break;
	default:
		if(checkAction(code, cardInReader, mobileCode))
			emit action(code);
		break;
	}
}

void ContainerPage::hideRightPane()
{
	ui->rightPane->hide();
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

void ContainerPage::showRightPane(ItemType itemType, const char *header)
{
	ui->rightPane->init(itemType, header);
	ui->rightPane->show();
}

void ContainerPage::showMainAction(const QList<Actions> &actions)
{
	if(!mainAction)
	{
		mainAction = std::make_unique<MainAction>(this);
		connect(mainAction.get(), &MainAction::action, this, &ContainerPage::forward);
	}
	mainAction->showActions(actions);
	bool isSignCard = actions.contains(SignatureAdd) || actions.contains(SignatureToken);
	bool isSignMobile = !isSignCard && (actions.contains(SignatureMobile) || actions.contains(SignatureSmartID));
	bool isEncrypt = actions.contains(EncryptContainer) && !ui->rightPane->findChildren<AddressItem*>().isEmpty();
	bool isDecrypt = actions.contains(DecryptContainer) || actions.contains(DecryptToken);
	mainAction->setButtonEnabled(isSupported && !hasEmptyFile &&
		(isEncrypt || isSignMobile || (!isBlocked && ((isSignCard && !isExpired) || isDecrypt))));
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
	else if(cardInReader.isEmpty())
		showMainAction({ SignatureMobile, SignatureSmartID });
	else if(isSeal)
		showMainAction({ SignatureToken, SignatureMobile, SignatureSmartID });
	else
		showMainAction({ SignatureAdd, SignatureMobile, SignatureSmartID });
}

void ContainerPage::transition(CryptoDoc *container, const QSslCertificate &cert)
{
	clear();
	isSupported = container && (container->state() & UnencryptedContainer || container->canDecrypt(cert));
	if(!container)
		return;
	setHeader(container->fileName());
	for(const CKey &key: container->keys())
		ui->rightPane->addWidget(new AddressItem(key, ui->rightPane, true));
	ui->leftPane->setModel(container->documentModel());
	updatePanes(container->state());
}

void ContainerPage::transition(DigiDoc* container)
{
	clear();
	emit action(ClearSignatureWarning);
	QMap<ria::qdigidoc4::WarningType, int> errors;
	setHeader(container->fileName());
	auto addError = [&errors](const SignatureItem* item) {
		int counter = errors.value(item->getError());
		errors[item->getError()] = ++counter;
	};

	if(!container->timestamps().isEmpty())
	{
		ui->rightPane->addHeader(QT_TRANSLATE_NOOP("ItemList", "Container timestamps"));

		for(const DigiDocSignature &c: container->timestamps())
		{
			auto *item = new SignatureItem(c, container->state(), ui->rightPane);
			if(c.isInvalid())
				addError(item);
			ui->rightPane->addHeaderWidget(item);
		}
	}

	for(const DigiDocSignature &c: container->signatures())
	{
		auto *item = new SignatureItem(c, container->state(), ui->rightPane);
		if(c.isInvalid())
			addError(item);
		ui->rightPane->addWidget(item);
	}

	for(auto i = errors.constBegin(); i != errors.constEnd(); ++i)
		emit warning(WarningText(i.key(), i.value()));
	if(container->fileName().endsWith(QStringLiteral("ddoc"), Qt::CaseInsensitive))
		emit warning(UnsupportedDDocWarning);
	if(container->isAsicS())
		emit warning(UnsupportedAsicSWarning);
	if(container->isCades())
		emit warning(UnsupportedAsicCadesWarning);

	hasEmptyFile = false;
	for (auto i = 0; i < container->documentModel()->rowCount(); i++)
	{
		if(container->documentModel()->fileSize(i) == 0)
		{
			emit warning(EmptyFileWarning);
			hasEmptyFile = true;
		}
	}

	isSupported = container->isSupported() || container->isPDF();
	showSigningButton();

	ui->leftPane->setModel(container->documentModel());
	updatePanes(container->state());
}

void ContainerPage::update(bool canDecrypt, CryptoDoc* container)
{
	isSupported = canDecrypt || container->state() & UnencryptedContainer;
	if(container && container->state() & EncryptedContainer)
		updateDecryptionButton();

	if(!container)
		return;

	hasEmptyFile = false;
	ui->rightPane->clear();
	for(const CKey &key: container->keys())
		ui->rightPane->addWidget(new AddressItem(key, ui->rightPane, true));
	if(container->state() & UnencryptedContainer)
		showMainAction({ EncryptContainer });
}

void ContainerPage::updateDecryptionButton()
{
	showMainAction({ isSeal ? DecryptToken : DecryptContainer });
}

void ContainerPage::updatePanes(ContainerState state)
{
	ui->leftPane->stateChange(state);
	ui->rightPane->stateChange(state);
	bool showPrintSummary = Settings::SHOW_PRINT_SUMMARY;
	auto setButtonsVisible = [](const QVector<QWidget*> &buttons, bool visible) {
		for(QWidget *button: buttons) button->setVisible(visible);
	};

	switch( state )
	{
	case UnsignedContainer:
		cancelText = QT_TR_NOOP("CANCEL");

		ui->changeLocation->show();
		ui->rightPane->clear();
		hideRightPane();
		ui->leftPane->init(fileName, QT_TRANSLATE_NOOP("ItemList", "Container files"));
		showSigningButton();
		setButtonsVisible({ ui->cancel, ui->convert, ui->save }, true);
		setButtonsVisible({ ui->saveAs, ui->email, ui->summary }, false);
		break;
	case UnsignedSavedContainer:
		cancelText = QT_TR_NOOP("STARTING");

		ui->changeLocation->show();
		ui->leftPane->init(fileName, QT_TRANSLATE_NOOP("ItemList", "Container files"));
		if( showPrintSummary )
			setButtonsVisible({ ui->cancel, ui->convert, ui->saveAs, ui->email, ui->summary }, true);
		else
			setButtonsVisible({ ui->cancel, ui->convert, ui->saveAs, ui->email }, true);
		setButtonsVisible({ ui->save }, false);
		showRightPane( ItemSignature, QT_TRANSLATE_NOOP("ItemList", "Container is not signed"));
		break;
	case SignedContainer:
		cancelText = QT_TR_NOOP("STARTING");

		ui->changeLocation->hide();
		ui->leftPane->init(fileName, QT_TRANSLATE_NOOP("ItemList", "Container files"));
		showRightPane(ItemSignature, QT_TRANSLATE_NOOP("ItemList", "Container signatures"));
		if( showPrintSummary )
			setButtonsVisible({ ui->cancel, ui->convert, ui->saveAs, ui->email, ui->summary }, true);
		else
			setButtonsVisible({ ui->cancel, ui->convert, ui->saveAs, ui->email }, true);
		setButtonsVisible({ ui->save }, false);
		break;
	case UnencryptedContainer:
		cancelText = QT_TR_NOOP("STARTING");
		convertText = QT_TR_NOOP("SIGN");

		ui->changeLocation->show();
		ui->leftPane->init(fileName);
		showRightPane(ItemAddress, QT_TRANSLATE_NOOP("ItemList", "Recipients"));
		showMainAction({ EncryptContainer });
		setButtonsVisible({ ui->cancel, ui->convert }, true);
		setButtonsVisible({ ui->save, ui->saveAs, ui->email, ui->summary }, false);
		break;
	case EncryptedContainer:
		cancelText = QT_TR_NOOP("STARTING");
		convertText = QT_TR_NOOP("SIGN");

		ui->changeLocation->hide();
		ui->leftPane->init(fileName, QT_TRANSLATE_NOOP("ItemList", "Encrypted files"));
		showRightPane(ItemAddress, QT_TRANSLATE_NOOP("ItemList", "Recipients"));
		updateDecryptionButton();
		setButtonsVisible({ ui->save, ui->summary }, false);
		setButtonsVisible({ ui->cancel, ui->convert, ui->saveAs, ui->email }, true);
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
	ui->changeLocation->setText(tr("CHANGE"));
	ui->changeLocation->setAccessibleName(ui->changeLocation->text().toLower());
	ui->cancel->setText(tr(cancelText));
	ui->cancel->setAccessibleName(tr(cancelText).toLower());
	ui->convert->setText(tr(convertText));
	ui->convert->setAccessibleName(tr(convertText).toLower());
}
