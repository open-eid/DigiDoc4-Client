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
#include "Settings.h"
#include "Styles.h"
#include "crypto/CryptoDoc.h"
#include "dialogs/AddRecipients.h"
#include "dialogs/MobileDialog.h"
#include "dialogs/WarningDialog.h"
#include "widgets/AddressItem.h"
#include "widgets/SignatureItem.h"

#include <QDir>
#include <QFileInfo>
#include <QFontMetrics>
#include <QMessageBox>

using namespace ria::qdigidoc4;


#define ACTION_WIDTH 200
#define ACTION_HEIGHT 65


ContainerPage::ContainerPage(QWidget *parent)
: QWidget(parent)
, ui(new Ui::ContainerPage)
, containerFont(Styles::font(Styles::Regular, 14))
, fm(QFontMetrics(containerFont))
, cancelText("CANCEL")
, changeLocationText("CHANGE")
, convertText("ENCRYPT")
, envelope("Envelope")
, canDecrypt(false)
, seal(false)
{
	ui->setupUi( this );
	init();
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
	AddRecipients dlg(ui->rightPane, qApp->activeWindow());
	auto rc = dlg.exec();
	if(rc && dlg.isUpdated())
		emit keysSelected(dlg.keys());
}

void ContainerPage::changeCard(const QString& idCode, bool seal)
{
	if(cardInReader != idCode)
	{
		cardInReader = idCode;
		this->seal = seal;
		if(mainAction && (ui->leftPane->getState() & SignatureContainers))
			showSigningButton();
		else if(ui->leftPane->getState() & EncryptedContainer)
			updateDecryptionButton();
	}
}

bool ContainerPage::checkAction(int code, const QString& selectedCard, const QString& selectedMobile)
{
	if(code == SignatureAdd || code == SignatureToken || code == SignatureMobile)
	{
		if(ui->rightPane->hasItem(
			[selectedCard, selectedMobile, code](Item* const item) -> bool
			{
				auto signatureItem = qobject_cast<SignatureItem* const>(item);
				return signatureItem && signatureItem->isSelfSigned(selectedCard, (code == SignatureAdd) ? QString() : selectedMobile);
			}
		))
		{
			WarningDialog dlg(tr("The document has already been signed by you."), this);
			dlg.addButton(tr("CONTINUE SIGNING"), SignatureAdd);
			dlg.exec();
			if(dlg.result() != SignatureAdd)
				return false;
		}
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
	hideOtherAction();
}

void ContainerPage::elideFileName(bool force)
{
	if(ui->containerFile->width() < containerFileWidth)
	{
		elided = true;
		ui->containerFile->setText("<a href='#browse-Container'><span style='color:rgb(53, 55, 57)'>" +
			fm.elidedText(fileName, Qt::ElideMiddle, ui->containerFile->width()) + "</span></a>");
	}
	else if(elided || force)
	{
		elided = false;
		ui->containerFile->setText("<a href='#browse-Container'><span style='color:rgb(53, 55, 57)'>" + fileName + "</span></a>");
	}
}

void ContainerPage::forward(int code)
{
	if(checkAction(code, cardInReader, mobileCode))
		emit action(code);
}

void ContainerPage::init()
{
	ui->leftPane->init(fileName);

	ui->container->setFont(containerFont);
	ui->containerFile->setFont(containerFont);

	ui->changeLocation->setIcons(QStringLiteral("/images/icon_Edit.svg"),
		QStringLiteral("/images/icon_Edit_hover.svg"), QStringLiteral("/images/icon_Edit_pressed.svg"), 4, 4, 18, 18);
	ui->changeLocation->init( LabelButton::BoxedDeepCeruleanWithCuriousBlue, tr("CHANGE"), Actions::ContainerLocation );
	ui->cancel->init( LabelButton::BoxedMojo, tr("CANCEL"), Actions::ContainerCancel );
	ui->convert->init( LabelButton::BoxedDeepCerulean, tr("ENCRYPT"), Actions::ContainerConvert );
	ui->saveAs->init( LabelButton::BoxedDeepCerulean, tr("SAVE AS"), Actions::ContainerSaveAs );
	ui->email->init( LabelButton::BoxedDeepCerulean, tr("SEND WITH E-MAIL"), Actions::ContainerEmail );
	ui->summary->init( LabelButton::BoxedDeepCerulean, tr("PRINT SUMMARY"), Actions::ContainerSummary );
	ui->save->init( LabelButton::BoxedDeepCerulean, tr("SAVE WITHOUT SIGNING"), Actions::ContainerSave );

	mobileCode = Settings().value(QStringLiteral("Client/MobileCode")).toString();

	connect(this, &ContainerPage::cardChanged, this, &ContainerPage::changeCard);
	connect(this, &ContainerPage::cardChanged, [this](const QString& idCode, bool seal, const QByteArray& serialNumber)
		{ emit ui->rightPane->idChanged(idCode, mobileCode, serialNumber); });
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

	ui->summary->setVisible(Settings(qApp->applicationName()).value(QStringLiteral("Client/ShowPrintSummary"), false).toBool());
}

void ContainerPage::initContainer( const QString &file, const QString &suffix )
{
	const QFileInfo f( file );
	if( !f.isFile() ) return;

	fileName = f.dir().path() + QDir::separator() + f.completeBaseName() + suffix;
	ui->containerFile->setText(fileName);
}

void ContainerPage::hideButtons(const QVector<QWidget*> &buttons)
{
	for( auto *button: buttons ) button->hide();
}

void ContainerPage::hideMainAction()
{
	if( mainAction )
	{
		mainAction->hide();
	}
	ui->mainActionSpacer->changeSize( 1, 20, QSizePolicy::Fixed );
	ui->navigationArea->layout()->invalidate();
}

void ContainerPage::hideOtherAction()
{
	if(!otherAction)
		return;

	otherAction->close();
	otherAction.reset();
	mainAction->setStyleSheet(QStringLiteral("QPushButton { border-top-left-radius: 2px; }"));
}

void ContainerPage::hideRightPane()
{
	ui->rightPane->hide();
}

void ContainerPage::mobileDialog()
{
	hideOtherAction();
	MobileDialog dlg(qApp->activeWindow());
	QString newCode = Settings().value(QStringLiteral("Client/MobileCode")).toString();
	if( dlg.exec() == QDialog::Accepted )
	{
		if(checkAction(SignatureMobile, dlg.idCode(), dlg.phoneNo()))
			emit action(SignatureMobile, dlg.idCode(), dlg.phoneNo());
	}

	if (newCode != mobileCode)
	{
		mobileCode = newCode;
		emit cardChanged(cardInReader, seal);
	}
}

void ContainerPage::resizeEvent( QResizeEvent *event )
{
	if( mainAction )
	{
		mainAction->move( this->width() - ACTION_WIDTH, this->height() - ACTION_HEIGHT );
	}
	if( otherAction )
	{
		otherAction->move( this->width() - ACTION_WIDTH, this->height() - ACTION_HEIGHT * 2 - 1 );
	}

	if(event->oldSize().width() != event->size().width())
		elideFileName();
}

void ContainerPage::changeEvent(QEvent* event)
{
	if (event->type() == QEvent::LanguageChange)
	{
		ui->retranslateUi(this);
		translateLabels();
		elideFileName(true);
	}

	QWidget::changeEvent(event);
}

void ContainerPage::setHeader(const QString &file)
{
	fileName = QDir::toNativeSeparators (file);
	window()->setWindowFilePath(fileName);
	window()->setWindowTitle(file.isEmpty() ? tr("DigiDoc4 client") : QFileInfo(file).fileName());
	containerFileWidth = fm.width(fileName);
	elideFileName(true);
}

void ContainerPage::showButtons(const QVector<QWidget*> &buttons)
{
	for( auto *button: buttons ) button->show();
}

void ContainerPage::showDropdown()
{
	if( otherAction )
	{
		hideOtherAction();
	}
	else
	{
		otherAction.reset( new MainAction( SignatureMobile, this ) );
		otherAction->move( this->width() - ACTION_WIDTH, this->height() - ACTION_HEIGHT * 2 - 1 );
		connect( otherAction.get(), &MainAction::action, this, &ContainerPage::mobileDialog );

		otherAction->show();
		mainAction->setStyleSheet(QStringLiteral("QPushButton { border-top-left-radius: 0px; }"));
		otherAction->setStyleSheet(QStringLiteral("QPushButton { border-top-left-radius: 2px; border-top-right-radius: 2px; }"));
	}

}

void ContainerPage::showRightPane(ItemType itemType, const QString &header)
{
	ui->rightPane->init(itemType, header);
	ui->rightPane->show();
}

void ContainerPage::showMainAction(Actions action)
{
	if( mainAction )
	{
		mainAction->update(action);
		mainAction->show();
	}
	else
	{
		mainAction.reset(new MainAction(action, this));
		mainAction->move( this->width() - ACTION_WIDTH, this->height() - ACTION_HEIGHT );
		mainAction->show();
	}

	for(const auto &conn: actionConnections)
		QObject::disconnect(conn);
	actionConnections.clear();
	if(action == SignatureMobile)
	{
		actionConnections.push_back(connect(mainAction.get(), &MainAction::action, this, &ContainerPage::mobileDialog));
	}
	else
	{
		actionConnections.push_back(connect(mainAction.get(), &MainAction::action, this, &ContainerPage::forward));
		actionConnections.push_back(connect(mainAction.get(), &MainAction::action, this, &ContainerPage::hideOtherAction));
		actionConnections.push_back(connect(mainAction.get(), &MainAction::dropdown, this, &ContainerPage::showDropdown));
	}

	ui->mainActionSpacer->changeSize( 198, 20, QSizePolicy::Fixed );
	ui->navigationArea->layout()->invalidate();
}

void ContainerPage::showSigningButton()
{
	hideOtherAction();
	if(cardInReader.isNull())
		showMainAction(SignatureMobile);
	else if(seal)
		showMainAction(SignatureToken);
	else
		showMainAction(SignatureAdd);
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
}

void ContainerPage::updateDecryptionButton()
{
	if(!canDecrypt || cardInReader.isNull())
		hideMainAction();
	else
		showMainAction(DecryptContainer);
}

void ContainerPage::updatePanes(ContainerState state)
{
	auto buttonWidth = ui->changeLocation->width();
	bool resize = false;
	bool showPrintSummary = Settings(qApp->applicationName()).value(QStringLiteral("Client/ShowPrintSummary"), false).toBool();

	switch( state )
	{
	case UnsignedContainer:
		cancelText = "CANCEL";

		ui->changeLocation->show();
		ui->rightPane->clear();
		hideRightPane();
		ui->leftPane->init(fileName, QStringLiteral("Content of the envelope"));
		showSigningButton();
		showButtons( { ui->cancel, ui->convert, ui->save } );
		hideButtons( { ui->saveAs, ui->email, ui->summary } );
		break;
	case UnsignedSavedContainer:
		cancelText = "STARTING";

		ui->changeLocation->show();
		ui->leftPane->init(fileName, QStringLiteral("Content of the envelope"));
		if( showPrintSummary )
			showButtons( { ui->cancel, ui->convert, ui->saveAs, ui->email, ui->summary } );
		else
			showButtons( { ui->cancel, ui->convert, ui->saveAs, ui->email } );
		hideButtons( { ui->save } );
		showRightPane( ItemSignature, QStringLiteral("Container is not signed"));
		break;
	case SignedContainer:
		cancelText = "STARTING";

		resize = !ui->changeLocation->isHidden();
		ui->changeLocation->hide();
		ui->leftPane->init(fileName, QStringLiteral("Content of the envelope"));
		showRightPane(ItemSignature, QStringLiteral("Container's signatures"));
		hideButtons( { ui->save } );
		if( showPrintSummary )
			showButtons( { ui->cancel, ui->convert, ui->saveAs, ui->email, ui->summary } );
		else
			showButtons( { ui->cancel, ui->convert, ui->saveAs, ui->email } );
		break;
	case UnencryptedContainer:
		cancelText = "STARTING";
		convertText = "SIGN";
		envelope = "Container";

		ui->changeLocation->show();
		ui->leftPane->init(fileName);
		showRightPane(ItemAddress, QStringLiteral("Recipients"));
		showMainAction(EncryptContainer);
		showButtons( { ui->cancel, ui->convert } );
		hideButtons( { ui->save, ui->saveAs, ui->email, ui->summary } );
		break;
	case EncryptedContainer:
		cancelText = "STARTING";
		convertText = "SIGN";
		envelope = "Container";

		resize = !ui->changeLocation->isHidden();
		ui->changeLocation->hide();
		ui->leftPane->init(fileName, QStringLiteral("Encrypted files"));
		showRightPane(ItemAddress, QStringLiteral("Recipients"));
		updateDecryptionButton();
		hideButtons( { ui->save, ui->summary } );
		showButtons( { ui->cancel, ui->convert, ui->saveAs, ui->email } );
		break;
	case DecryptedContainer:
		cancelText = "STARTING";
		envelope = "Container";

		resize = !ui->changeLocation->isHidden();
		ui->changeLocation->hide();
		ui->leftPane->init(fileName, QStringLiteral("Decrypted files"));
		showRightPane(ItemAddress, QStringLiteral("Recipients"));
		hideMainAction();
		hideButtons( { ui->convert, ui->save, ui->summary } );
		showButtons( { ui->cancel, ui->saveAs, ui->email } );
		break;
	default:
		// Uninitialized cannot be shown on container page
		break;
	}

	if(resize)
	{
		// Forcibly resize the filename widget after hiding button
		ui->containerFile->resize(ui->containerFile->width() + buttonWidth,
		ui->containerFile->height());
	}

	translateLabels();
}

void ContainerPage::togglePrinting(bool enable)
{
	ui->summary->setVisible(enable);
}

void ContainerPage::translateLabels()
{
	ui->changeLocation->setText(tr(qPrintable(changeLocationText)));
	ui->cancel->setText(tr(qPrintable(cancelText)));
	ui->container->setText(tr(qPrintable(envelope)));
	ui->convert->setText(tr(qPrintable(convertText)));
	ui->saveAs->setText(tr("SAVE AS"));
	ui->email->setText(tr("SEND WITH E-MAIL"));
	ui->summary->setText(tr("PRINT SUMMARY"));
	ui->save->setText(tr("SAVE WITHOUT SIGNING"));
}
