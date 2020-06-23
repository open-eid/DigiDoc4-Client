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

#include "DigiDoc.h"
#include "Styles.h"
#include "crypto/CryptoDoc.h"
#include "dialogs/AddRecipients.h"
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

#include <QtCore/QSettings>

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

	mobileCode = QSettings().value(QStringLiteral("MobileCode")).toString();

	connect(this, &ContainerPage::moved,this, &ContainerPage::setHeader);
	connect(this, &ContainerPage::details, ui->rightPane, &ItemList::details);
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

	ui->summary->setVisible(QSettings().value(QStringLiteral("ShowPrintSummary"), false).toBool());
}

ContainerPage::~ContainerPage()
{
	delete ui;
}

void ContainerPage::addError(const SignatureItem* item, QMap<ria::qdigidoc4::WarningType, int> &errors)
{
	auto counter = errors.value(item->getError());
	counter++;
	errors[item->getError()] = counter;
}

void ContainerPage::addressSearch()
{
	AddRecipients dlg(ui->rightPane, topLevelWidget());
	if(dlg.exec() && dlg.isUpdated())
		emit keysSelected(dlg.keys());
}

void ContainerPage::cardChanged(const QString &idCode, bool isSeal, bool isExpired, bool isBlocked, const QByteArray &serialNumber)
{
	emit ui->rightPane->idChanged(idCode, serialNumber);
	if(cardInReader == idCode)
		return;

	cardInReader = idCode;
	this->seal = isSeal;
	this->isExpired = isExpired;
	this->isBlocked = isBlocked;
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
				auto signatureItem = qobject_cast<SignatureItem* const>(item);
				return signatureItem && signatureItem->isSelfSigned(selectedCard, (code == SignatureMobile) ? selectedMobile: QString());
			}
		))
		{
			WarningDialog dlg(tr("The document has already been signed by you."), this);
			dlg.addButton(tr("CONTINUE SIGNING"), SignatureAdd);
			return dlg.exec() == SignatureAdd;
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
	canDecrypt = false;
}

void ContainerPage::clearPopups()
{
	if(mainAction) mainAction->hideDropdown();
}

void ContainerPage::elideFileName(bool force)
{
	if(ui->containerFile->width() < containerFileWidth)
	{
		elided = true;
		ui->containerFile->setText("<a href='#browse-Container'><span style='color:rgb(53, 55, 57)'>" +
			ui->containerFile->fontMetrics().elidedText(fileName.toHtmlEscaped(), Qt::ElideMiddle, ui->containerFile->width()) + "</span></a>");
	}
	else if(elided || force)
	{
		elided = false;
		ui->containerFile->setText("<a href='#browse-Container'><span style='color:rgb(53, 55, 57)'>" + fileName.toHtmlEscaped() + "</span></a>");
	}
}

bool ContainerPage::eventFilter(QObject *o, QEvent *e)
{
	switch(e->type())
	{
	case QEvent::Resize:
	case QEvent::LanguageChange:
		if(o == ui->containerFile)
			elideFileName(true);
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
		MobileDialog dlg(topLevelWidget());
		QString newCode = QSettings().value(QStringLiteral("MobileCode")).toString();
		if(dlg.exec() == QDialog::Accepted)
		{
			if(checkAction(SignatureMobile, dlg.idCode(), dlg.phoneNo()))
				emit action(SignatureMobile, dlg.idCode(), dlg.phoneNo());
		}

		if (newCode != mobileCode)
		{
			mobileCode = newCode;
			cardChanged(cardInReader, seal, isExpired, isBlocked);
		}
		break;
	}
	case SignatureSmartID:
	{
		SmartIDDialog dlg(topLevelWidget());
		QString newCode = QSettings().value(QStringLiteral("SmartID")).toString();
		if(dlg.exec() == QDialog::Accepted)
		{
			if(checkAction(SignatureMobile, dlg.idCode(), QString()))
				emit action(SignatureSmartID, dlg.country(), dlg.idCode());
		}

		if (newCode != mobileCode)
		{
			mobileCode = newCode;
			cardChanged(cardInReader, seal, isExpired, isBlocked);
		}
		break;
	}
	case ContainerCancel:
		window()->setWindowFilePath(QString());
		window()->setWindowTitle(tr("DigiDoc4 client"));
		emit action(code);
		break;
	default:
		if(checkAction(code, cardInReader, mobileCode))
			emit action(code);
		break;
	}
}

void ContainerPage::initContainer( const QString &file, const QString &suffix )
{
	const QFileInfo f( file );
	if( !f.isFile() ) return;

	fileName = (f.dir().path() + QDir::separator() + f.completeBaseName() + suffix);
	ui->containerFile->setText(fileName.toHtmlEscaped());
}

void ContainerPage::hideMainAction()
{
	if(mainAction)
		mainAction->hide();
	ui->mainActionSpacer->changeSize( 1, 20, QSizePolicy::Fixed );
	ui->navigationArea->layout()->invalidate();
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
	window()->setWindowTitle(file.isEmpty() ? tr("DigiDoc4 client") : QFileInfo(file).fileName());
	elideFileName(true);
}

void ContainerPage::showRightPane(ItemType itemType, const QString &header)
{
	ui->rightPane->init(itemType, header);
	ui->rightPane->show();
}

void ContainerPage::showMainAction(const QList<Actions> &actions)
{
	if(!mainAction)
	{
		mainAction.reset(new MainAction(this));
		connect(mainAction.get(), &MainAction::action, this, &ContainerPage::forward);
	}
	mainAction->update(actions);
	bool isSign =
		actions.contains(SignatureAdd) || actions.contains(SignatureToken) ||
		actions.contains(SignatureMobile) || actions.contains(SignatureSmartID);
	bool isEncrypt =
		actions.contains(EncryptContainer) && !ui->rightPane->findChildren<AddressItem*>().isEmpty();
	bool isDecrypt =
		actions.contains(DecryptContainer) || actions.contains(DecryptToken);
	mainAction->setButtonEnabled(isEncrypt || (!isBlocked && ((isSign && !isExpired) || isDecrypt)));
	ui->mainActionSpacer->changeSize( 198, 20, QSizePolicy::Fixed );
	ui->navigationArea->layout()->invalidate();
}

void ContainerPage::showSigningButton()
{
	if(cardInReader.isNull())
		showMainAction({ SignatureMobile, SignatureSmartID });
	else if(seal)
		showMainAction({ SignatureToken, SignatureMobile, SignatureSmartID });
	else
		showMainAction({ SignatureAdd, SignatureMobile, SignatureSmartID });
}

void ContainerPage::transition(CryptoDoc* container, bool canDecrypt)
{
	clear();
	this->canDecrypt = canDecrypt;
	ContainerState state = container->state();
	ui->leftPane->stateChange(state);
	ui->rightPane->stateChange(state);

	setHeader(container->fileName());

	for(const CKey &key: container->keys())
		ui->rightPane->addWidget(new AddressItem(key, ui->rightPane, true));

	ui->leftPane->setModel(container->documentModel());
	updatePanes(state);
}

void ContainerPage::transition(DigiDoc* container)
{
	clear();
	emit action(ClearSignatureWarning);

	ContainerState state = container->state();
	QMap<ria::qdigidoc4::WarningType, int> errors;
	ui->leftPane->stateChange(state);
	ui->rightPane->stateChange(state);

	setHeader(container->fileName());

	if(!container->timestamps().isEmpty())
	{
		ui->rightPane->addHeader(QStringLiteral("Container's timestamps"));

		for(const DigiDocSignature &c: container->timestamps())
		{
			SignatureItem *item = new SignatureItem(c, state, false, ui->rightPane);
			if(item->isInvalid())
				addError(item, errors);
			ui->rightPane->addHeaderWidget(item);
		}
	}

	bool enableSigning = container->isSupported();
	for(const DigiDocSignature &c: container->signatures())
	{
		SignatureItem *item = new SignatureItem(c, state, enableSigning, ui->rightPane);
		if(item->isInvalid())
			addError(item, errors);
		ui->rightPane->addWidget(item);
	}

	if(!errors.isEmpty())
	{
		QMap<ria::qdigidoc4::WarningType, int>::const_iterator i;
		for (i = errors.constBegin(); i != errors.constEnd(); ++i)
			emit warning(WarningText(i.key(), i.value()));
	}

	if(enableSigning || container->isService())
		showSigningButton();
	else
		hideMainAction();

	ui->leftPane->setModel(container->documentModel());
	updatePanes(state);
}

void ContainerPage::update(bool canDecrypt, CryptoDoc* container)
{
	this->canDecrypt = canDecrypt;
	if(ui->leftPane->getState() & EncryptedContainer)
		updateDecryptionButton();

	if(!container)
		return;

	ui->rightPane->clear();
	for(const CKey &key: container->keys())
		ui->rightPane->addWidget(new AddressItem(key, ui->rightPane, true));
	if(ui->leftPane->getState() & UnencryptedContainer)
		showMainAction({ EncryptContainer });
}

void ContainerPage::updateDecryptionButton()
{
	if(!canDecrypt || cardInReader.isNull())
		hideMainAction();
	else if(seal)
		showMainAction({ DecryptToken });
	else
		showMainAction({ DecryptContainer });
}

void ContainerPage::updatePanes(ContainerState state)
{
	bool showPrintSummary = QSettings().value(QStringLiteral("ShowPrintSummary"), false).toBool();
	auto setButtonsVisible = [](const QVector<QWidget*> &buttons, bool visible) {
		for(QWidget *button: buttons) button->setVisible(visible);
	};

	switch( state )
	{
	case UnsignedContainer:
		cancelText = "CANCEL";

		ui->changeLocation->show();
		ui->rightPane->clear();
		hideRightPane();
		ui->leftPane->init(fileName, QStringLiteral("Content of the envelope"));
		showSigningButton();
		setButtonsVisible({ ui->cancel, ui->convert, ui->save }, true);
		setButtonsVisible({ ui->saveAs, ui->email, ui->summary }, false);
		break;
	case UnsignedSavedContainer:
		cancelText = "STARTING";

		ui->changeLocation->show();
		ui->leftPane->init(fileName, QStringLiteral("Content of the envelope"));
		if( showPrintSummary )
			setButtonsVisible({ ui->cancel, ui->convert, ui->saveAs, ui->email, ui->summary }, true);
		else
			setButtonsVisible({ ui->cancel, ui->convert, ui->saveAs, ui->email }, true);
		setButtonsVisible({ ui->save }, false);
		showRightPane( ItemSignature, QStringLiteral("Container is not signed"));
		break;
	case SignedContainer:
		cancelText = "STARTING";

		ui->changeLocation->hide();
		ui->leftPane->init(fileName, QStringLiteral("Content of the envelope"));
		showRightPane(ItemSignature, QStringLiteral("Container's signatures"));
		if( showPrintSummary )
			setButtonsVisible({ ui->cancel, ui->convert, ui->saveAs, ui->email, ui->summary }, true);
		else
			setButtonsVisible({ ui->cancel, ui->convert, ui->saveAs, ui->email }, true);
		setButtonsVisible({ ui->save }, false);
		break;
	case UnencryptedContainer:
		cancelText = "STARTING";
		convertText = "SIGN";

		ui->changeLocation->show();
		ui->leftPane->init(fileName);
		showRightPane(ItemAddress, QStringLiteral("Recipients"));
		showMainAction({ EncryptContainer });
		setButtonsVisible({ ui->cancel, ui->convert }, true);
		setButtonsVisible({ ui->save, ui->saveAs, ui->email, ui->summary }, false);
		break;
	case EncryptedContainer:
		cancelText = "STARTING";
		convertText = "SIGN";

		ui->changeLocation->hide();
		ui->leftPane->init(fileName, QStringLiteral("Encrypted files"));
		showRightPane(ItemAddress, QStringLiteral("Recipients"));
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
	tr("STARTING");
	tr("SIGN");
	ui->changeLocation->setText(tr("CHANGE"));
	ui->changeLocation->setAccessibleName(ui->changeLocation->text().toLower());
	ui->cancel->setText(tr(cancelText));
	ui->cancel->setAccessibleName(tr(cancelText).toLower());
	ui->convert->setText(tr(convertText));
	ui->convert->setAccessibleName(tr(convertText).toLower());
}
