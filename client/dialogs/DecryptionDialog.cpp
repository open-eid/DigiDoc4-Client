// SPDX-FileCopyrightText: Estonian Information System Authority
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "DecryptionDialog.h"
#include "ui_DecryptionDialog.h"

#include "Application.h"
#include "CryptoDoc.h"
#include "QCryptoBackend.h"
#include "SslCertificate.h"
#include "dialogs/PinLogin.h"
#include "dialogs/WarningDialog.h"
#include "effects/Overlay.h"
#include "widgets/CardListItem.h"

#include <QtCore/QPointer>

#include <memory>

DecryptionDialog::DecryptionDialog(const CKey &key, const QString &recipient, QWidget *parent)
	: QDialog(parent)
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

	ui->noCardIcon->load(QStringLiteral(":/images/icon_IDkaart_red.svg"));
	updateCards();
	connect(qApp->cryptoManager(), &QCryptoManager::cacheChanged, this, &DecryptionDialog::updateCards);
	connect(ui->pin, &QLineEdit::textEdited, this, [this] {
		PinLogin::clearError(ui);
		ui->decrypt->setEnabled(ui->pin->hasAcceptableInput());
	});
	connect(ui->pin, &QLineEdit::returnPressed, ui->decrypt, &QPushButton::click);

	connect(ui->decrypt, &QPushButton::clicked, this, [this] {
		if(performLogin())
			accept();
	});
	connect(ui->cancel, &QPushButton::clicked, this, &QDialog::reject);
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

void DecryptionDialog::changeEvent(QEvent *event)
{
	if(event->type() == QEvent::LanguageChange)
	{
		ui->retranslateUi(this);
		updateNoCard();
		updatePinControls();
	}
	QDialog::changeEvent(event);
}

void DecryptionDialog::reject()
{
	if(!ui->progressPage->isVisible())
		QDialog::reject();
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
	PinLogin::updateSelection(ui, m_selectedToken);
	updatePinControls();
}

void DecryptionDialog::updatePinControls()
{
	ui->decrypt->setEnabled(PinLogin::update(ui, m_selectedToken));
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
