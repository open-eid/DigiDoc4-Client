// SPDX-FileCopyrightText: Estonian Information System Authority
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "DecryptionDialog.h"
#include "ui_DecryptionDialog.h"

#include "Application.h"
#include "CDocSupport.h"
#include "CryptoDoc.h"
#include "IKValidator.h"
#include "QCryptoBackend.h"
#include "SslCertificate.h"
#include "dialogs/PinLogin.h"
#include "dialogs/WarningDialog.h"
#include "effects/Overlay.h"
#include "widgets/CardListItem.h"

#include <QtCore/QPointer>
#include <QtCore/QTimer>

#include <memory>

DecryptionDialog::DecryptionDialog(CryptoDoc *doc, const CKey &key, const QString &recipient, QWidget *parent)
	: QDialog(parent)
	, cryptoDoc(doc)
	, m_overlay(std::make_unique<Overlay>(this))
	, m_key(key)
	, ui(new Ui::DecryptionDialog)
{
	ui->setupUi(this);
	ui->progressPage->hide();
	setWindowFlags(Qt::Dialog | Qt::CustomizeWindowHint);
#ifdef Q_OS_WIN
	ui->buttonLayout->setDirection(QBoxLayout::RightToLeft);
#endif

	ui->recipientIcon->load(QStringLiteral(":/images/icon_Minu_eID_hover.svg"));
	ui->recipientName->setText(recipient);

	ui->tabs->addTab({});
	ui->tabs->addTab({});
	ui->tabs->addTab({});
	updateTabs();
	// Tab bar: a card's lock opens only with the card, a key share only with
	// Mobile-ID or Smart-ID, so the methods that cannot open this lock are hidden
	const bool share = m_key.lock.type == libcdoc::Lock::SHARE_SERVER;
	// Checked on each opening, so an updated central configuration is picked up
	m_sharesKnown = !share || DDConfiguration::shareServersKnown(m_key.lock.getString(libcdoc::Lock::SHARE_URLS));
	ui->errorShares->setVisible(!m_sharesKnown);
	ui->tabs->setTabVisible(IDCard, !share);
	ui->tabs->setTabVisible(MobileID, share);
	ui->tabs->setTabVisible(SmartID, share);
	ui->tabs->setVisible(share);
	connect(ui->tabs, &QTabBar::currentChanged, ui->tabStack, &QStackedWidget::setCurrentIndex);
	connect(ui->tabs, &QTabBar::currentChanged, this, &DecryptionDialog::updateShareButton);
	ui->tabs->setCurrentIndex(share ? MobileID : IDCard);
	ui->tabStack->setCurrentIndex(ui->tabs->currentIndex());

	// ID-card tab
	ui->noCardIcon->load(QStringLiteral(":/images/icon_IDkaart_red.svg"));
	updateCards();
	connect(qApp->cryptoManager(), &QCryptoManager::cacheChanged, this, &DecryptionDialog::updateCards);
	connect(ui->pin, &QLineEdit::textEdited, this, [this] {
		PinLogin::clearError(ui);
		ui->decrypt->setEnabled(ui->pin->hasAcceptableInput());
	});
	connect(ui->pin, &QLineEdit::returnPressed, ui->decrypt, &QPushButton::click);

	// Mobile-ID and Smart-ID tabs: the lock names the recipient, shown in the header,
	// so only the Mobile-ID phone number is asked for
	ui->phoneNo->setValidator(new NumberValidator(ui->phoneNo));
	ui->phoneNo->setAttribute(Qt::WA_MacShowFocusRect, false);
	ui->errorPhone->hide();
	connect(ui->phoneNo, &QLineEdit::textEdited, this, &DecryptionDialog::updateShareButton);
	connect(ui->phoneNo, &QLineEdit::returnPressed, ui->decrypt, &QPushButton::click);

	connect(ui->decrypt, &QPushButton::clicked, this, [this] {
		if(currentMethod() == IDCard ? performLogin() : performShareDecrypt())
			accept();
	});
	connect(ui->progressCancel, &QPushButton::clicked, this, [this] {
		ui->progressCancel->setEnabled(false);
		cryptoDoc->cancel();
	});
	connect(ui->cancel, &QPushButton::clicked, this, &QDialog::reject);
	if(share)
		updateShareButton();
	connect(this, &QDialog::finished, this, &QDialog::close);
}

DecryptionDialog::~DecryptionDialog()
{
	delete ui;
}

std::unique_ptr<QCryptoBackend> DecryptionDialog::takeBackend()
{
	return std::move(m_backend);
}

DecryptionDialog::Method DecryptionDialog::currentMethod() const
{
	return static_cast<Method>(ui->tabs->currentIndex());
}

void DecryptionDialog::changeEvent(QEvent *event)
{
	if(event->type() == QEvent::LanguageChange)
	{
		ui->retranslateUi(this);
		updateTabs();
		updateNoCard();
		updatePinControls();
		if(currentMethod() != IDCard)
			updateProgressInfo();
	}
	QDialog::changeEvent(event);
}

void DecryptionDialog::reject()
{
	// A PIN pad entry is shown without Cancel and runs on the reader until it ends there
	if(ui->progressPage->isVisible())
	{
		if(ui->progressCancel->isVisible() && ui->progressCancel->isEnabled())
			ui->progressCancel->click();
		return;
	}
	QDialog::reject();
}

void DecryptionDialog::updateTabs()
{
	ui->tabs->setTabText(IDCard, tr("ID-card"));
	ui->tabs->setTabText(MobileID, tr("Mobile-ID"));
	ui->tabs->setTabText(SmartID, tr("Smart-ID"));
}

void DecryptionDialog::updateProgressInfo()
{
	if(currentMethod() == MobileID)
		ui->progressInfo->setText(m_confirmDecrypt ?
			tr("Confirm decryption: make sure the control code matches the one on your phone screen and enter your Mobile-ID PIN1.") :
			tr("Log in: make sure the control code matches the one on your phone screen and enter your Mobile-ID PIN1."));
	else
		ui->progressInfo->setText(m_confirmDecrypt ?
			tr("Confirm decryption: make sure the control code matches the one in your Smart-ID app and enter your PIN1.") :
			tr("Log in: make sure the control code matches the one in your Smart-ID app and enter your PIN1."));
}

void DecryptionDialog::updateNoCard()
{
	ui->noCardStatus->setText(CardList::emptyText());
	ui->noCard->setAccessibleDescription(ui->noCardStatus->text());
}

void DecryptionDialog::updateCards()
{
	qDeleteAll(ui->cardListContainer->findChildren<CardListItem*>());

	auto usableToken = [this](const TokenData &token) {
		SslCertificate c(token.cert());
		return c.isValid() && !token.data(QStringLiteral("blocked")).toBool() && m_key.canDecrypt(token.cert());
	};

	const QList<TokenData> tokens = qApp->cryptoManager()->cache();
	if(std::none_of(tokens.cbegin(), tokens.cend(),
		[this](const TokenData &t) { return t == m_selectedToken; }))
	{
		m_selectedToken = {};
		for(const TokenData &token: tokens)
		{
			if(!SslCertificate(token.cert()).isEncryptionCert())
				continue;
			if(m_selectedToken.isNull())
				m_selectedToken = token;
			if(usableToken(token))
			{
				m_selectedToken = token;
				break;
			}
		}
	}

	bool any = false;
	CardListItem *selectedCard = nullptr;
	for(const TokenData &token: tokens) {
		if(!SslCertificate(token.cert()).isEncryptionCert())
			continue;
		auto *w = new CardListItem(ui->cardListContainer);
		w->update(token, token == m_selectedToken, m_key.canDecrypt(token.cert()) ?
			nullptr : QT_TRANSLATE_NOOP("CardListItem", "This card cannot decrypt the container"));
		connect(w, &CardListItem::selected, this, &DecryptionDialog::updateSelection);
		ui->cardListContainer->layout()->addWidget(w);
		w->show();
		any = true;
		if(token == m_selectedToken)
			selectedCard = w;
	}

	if(!any)
		updateNoCard();
	ui->noCard->setVisible(!any);
	ui->cardList->setVisible(any);
	if(selectedCard)
		QMetaObject::invokeMethod(ui->cardList, [list = ui->cardList, card = QPointer<CardListItem>(selectedCard)] {
			if(card)
				list->ensureWidgetVisible(card, 0, 0);
		}, Qt::QueuedConnection);
	ui->labelCert->setVisible(any);
	updateSelection();
}

void DecryptionDialog::updateSelection()
{
	PinLogin::updateSelection(ui, m_selectedToken, currentMethod() == IDCard);
	updatePinControls();
}

void DecryptionDialog::updatePinControls()
{
	const bool ready = PinLogin::update(ui, m_selectedToken);
	if(currentMethod() == IDCard)
		ui->decrypt->setEnabled(ready);
}

bool DecryptionDialog::performLogin()
{
	auto val = PinLogin::login(ui, m_selectedToken);
	if(!val) {
		switch(val.error()) {
		case QCryptoBackend::PinIncorrect:
			updatePinControls();
			return false;
		case QCryptoBackend::PinCanceled:
			return false;
		default:
			WarningDialog::create(this)
				->withTitle(tr("Failed to decrypt document"))
				->withText(QCryptoBackend::errorString(val.error()))
				->exec();
			return false;
		}
	}
	m_backend.reset(val.value());
	return true;
}

void DecryptionDialog::updateShareButton()
{
	static const QStringList countryCodes {QStringLiteral("372"), QStringLiteral("370")};
	const QString phone = ui->phoneNo->text();
	ui->decrypt->setEnabled(m_sharesKnown && (currentMethod() == SmartID ||
		(phone.size() >= 8 && countryCodes.contains(phone.left(3)))));
}

bool DecryptionDialog::performShareDecrypt()
{
	auto showPage = [this](QWidget *page) {
		ui->formPage->setVisible(page == ui->formPage);
		ui->progressPage->setVisible(page == ui->progressPage);
	};
	ui->progressCode->clear();
	m_confirmDecrypt = false;
	updateProgressInfo();
	ui->progressBar->setValue(0);
	ui->progressCancel->setEnabled(true);
	showPage(ui->progressPage);

	// Each confirmation on the device gets its own code and its own time to answer
	QPointer<QTimer> timer = new QTimer(this);
	timer->setInterval(1000);
	connect(timer, &QTimer::timeout, this, [this] {
		ui->progressBar->setValue(std::min(ui->progressBar->value() + 1, ui->progressBar->maximum()));
	});
	QPointer<DecryptionDialog> self(this);
	auto feedback = [self, timer](int code) {
		if(!self || !timer)
			return;
		if(!self->ui->progressCode->text().isEmpty()) {
			self->m_confirmDecrypt = true;
			self->updateProgressInfo();
		}
		self->ui->progressCode->setText(QStringLiteral("%1").arg(code, 4, 10, QLatin1Char('0')));
		self->ui->progressBar->setValue(0);
		timer->start();
	};
	bool result = cryptoDoc->decrypt(m_key.lock,
		currentMethod() == MobileID ? ui->phoneNo->text() : QString(), feedback);
	if(!self)
		return result;
	timer->deleteLater();
	if(!result)
		showPage(ui->formPage);
	return result;
}
