// SPDX-FileCopyrightText: Estonian Information System Authority
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "SigningDialog.h"
#include "ui_SigningDialog.h"

#include "Application.h"
#include "DigiDoc.h"
#include "IKValidator.h"
#include "QPCSC.h"
#include "QCryptoBackend.h"
#include "QSmartCard.h"
#include "Settings.h"
#include "SslCertificate.h"
#include "dialogs/WarningDialog.h"
#include "effects/Overlay.h"
#include "signer/QSigner.h"
#include "signer/MobileProgress.h"
#include "signer/SmartIDProgress.h"
#include "widgets/CardListItem.h"

#include <QtCore/QPointer>
#include <QtWidgets/QMessageBox>

static void setError(LineEdit *input, QLabel *error, const QString &msg)
{
	input->setLabel(msg.isEmpty() ? QString() : QStringLiteral("error"));
	error->setText(msg);
	error->setHidden(msg.isEmpty());
}

SigningDialog::SigningDialog(DigiDoc *doc, QWidget *parent)
	: QDialog(parent)
	, digiDoc(doc)
	, ui(new Ui::SigningDialog)
{
	new Overlay(this);
	ui->setupUi(this);
	ui->progressPage->hide();
	setWindowFlags(Qt::Dialog | Qt::CustomizeWindowHint);
#ifdef Q_OS_WIN
	ui->buttonLayout->setDirection(QBoxLayout::RightToLeft);
#endif

	// Tab bar
	ui->tabs->addTab({});
	ui->tabs->addTab({});
	ui->tabs->addTab({});
	updateTabs();
	connect(ui->tabs, &QTabBar::currentChanged, ui->tabStack, &QStackedWidget::setCurrentIndex);

	// ID-card tab
	ui->noCardIcon->load(QStringLiteral(":/images/icon_IDkaart_red.svg"));
	updateCards(qApp->cryptoManager()->cache());
	connect(qApp->cryptoManager(), &QCryptoManager::cacheChanged, this, &SigningDialog::updateCards);
	connect(ui->pin, &QLineEdit::textEdited, this, [this] {
		setError(ui->pin, ui->errorPin, {});
		ui->sign->setEnabled(ui->pin->hasAcceptableInput());
	});
	connect(ui->pin, &QLineEdit::returnPressed, ui->sign, &QPushButton::click);
	connect(ui->tabs, &QTabBar::currentChanged, this, [this](int index) {
		if(index == IDCard)
			updateSelection();
		else
			ui->sign->setEnabled(true);
	});

	// Mobile-ID tab
	ui->idCodeMID->setValidator(new NumberValidator(ui->idCodeMID));
	ui->idCodeMID->setText(Settings::MOBILEID_CODE);
	ui->idCodeMID->setAttribute(Qt::WA_MacShowFocusRect, false);
	ui->errorCodeMID->hide();
	ui->phoneNo->setValidator(new NumberValidator(ui->phoneNo));
	ui->phoneNo->setText(Settings::MOBILEID_NUMBER);
	ui->phoneNo->setAttribute(Qt::WA_MacShowFocusRect, false);
	ui->errorPhone->hide();
	ui->cbRememberMID->setChecked(Settings::MOBILEID_REMEMBER);
	ui->cbRememberMID->setAttribute(Qt::WA_MacShowFocusRect, false);
	auto saveMIDSettings = [this] {
		bool checked = ui->cbRememberMID->isChecked();
		Settings::MOBILEID_REMEMBER = checked;
		Settings::MOBILEID_CODE = checked ? ui->idCodeMID->text() : QString();
		Settings::MOBILEID_NUMBER = checked ? ui->phoneNo->text() : QString();
	};
	connect(ui->idCodeMID, &QLineEdit::returnPressed, ui->sign, &QPushButton::click);
	connect(ui->idCodeMID, &QLineEdit::textEdited, this, saveMIDSettings);
	connect(ui->idCodeMID, &QLineEdit::textEdited, this, [this] {
		setError(ui->idCodeMID, ui->errorCodeMID, {});
	});
	connect(ui->phoneNo, &QLineEdit::returnPressed, ui->sign, &QPushButton::click);
	connect(ui->phoneNo, &QLineEdit::textEdited, this, saveMIDSettings);
	connect(ui->phoneNo, &QLineEdit::textEdited, this, [this] {
		setError(ui->phoneNo, ui->errorPhone, {});
	});
	connect(ui->cbRememberMID, &QCheckBox::clicked, this, saveMIDSettings);

	// Smart-ID tab
	static const QString &EE = Settings::SMARTID_COUNTRY_LIST.first();
	auto *ik = new NumberValidator(ui->idCodeSID);
	ui->idCodeSID->setValidator(Settings::SMARTID_COUNTRY == EE ? ik : nullptr);
	ui->idCodeSID->setText(Settings::SMARTID_CODE);
	ui->idCodeSID->setAttribute(Qt::WA_MacShowFocusRect, false);
	ui->errorCodeSID->hide();
	ui->cbRememberSID->setAttribute(Qt::WA_MacShowFocusRect, false);
	for(int i = 0, count = Settings::SMARTID_COUNTRY_LIST.size(); i < count; ++i)
		ui->idCountry->setItemData(i, Settings::SMARTID_COUNTRY_LIST[i]);
	ui->idCountry->setCurrentIndex(ui->idCountry->findData(Settings::SMARTID_COUNTRY));
	ui->cbRememberSID->setChecked(Settings::SMARTID_REMEMBER);
	auto saveSIDSettings = [this] {
		bool checked = ui->cbRememberSID->isChecked();
		Settings::SMARTID_REMEMBER = checked;
		Settings::SMARTID_CODE = checked ? ui->idCodeSID->text() : QString();
		Settings::SMARTID_COUNTRY = checked ? ui->idCountry->currentData().toString() : Settings::SMARTID_COUNTRY_LIST.first();
	};
	connect(ui->idCodeSID, &QLineEdit::returnPressed, ui->sign, &QPushButton::click);
	connect(ui->idCodeSID, &QLineEdit::textEdited, this, saveSIDSettings);
	connect(ui->idCodeSID, &QLineEdit::textEdited, this, [this] {
		setError(ui->idCodeSID, ui->errorCodeSID, {});
	});
	connect(ui->idCountry, &QComboBox::currentTextChanged, this, [this, ik, saveSIDSettings] {
		static const QString &EE2 = Settings::SMARTID_COUNTRY_LIST.first();
		ui->idCodeSID->setValidator(ui->idCountry->currentData().toString() == EE2 ? ik : nullptr);
		saveSIDSettings();
	});
	connect(ui->cbRememberSID, &QCheckBox::clicked, this, saveSIDSettings);

	connect(ui->sign, &QPushButton::clicked, this, &SigningDialog::performSign);
	connect(ui->cancel, &QPushButton::clicked, this, &QDialog::reject);
	connect(this, &QDialog::finished, this, &QDialog::close);
}

SigningDialog::~SigningDialog()
{
	delete ui;
}

void SigningDialog::setRoleAddress(const QString &city, const QString &country,
	const QString &state, const QString &zip, const QString &role)
{
	m_city = city;
	m_country = country;
	m_state = state;
	m_zip = zip;
	m_role = role;
}

SigningDialog::Method SigningDialog::currentMethod() const
{
	return static_cast<Method>(ui->tabs->currentIndex());
}

bool SigningDialog::confirmAlreadySigned()
{
	QString code;
	switch(currentMethod()) {
	case IDCard: code = SslCertificate(m_selectedToken.cert()).personalCode(); break;
	case MobileID: code = ui->idCodeMID->text(); break;
	case SmartID: code = ui->idCodeSID->text(); break;
	}
	if(code.isEmpty() || std::none_of(digiDoc->signatures().cbegin(), digiDoc->signatures().cend(),
			[&code](const DigiDocSignature &sig) { return SslCertificate(sig.cert()).personalCode() == code; }))
		return true;
	return WarningDialog::create(this)
		->withTitle(tr("The document has already been signed by you"))
		->addButton(tr("Continue signing"), QMessageBox::Ok)
		->exec() == QMessageBox::Ok;
}

void SigningDialog::changeEvent(QEvent *event)
{
	if(event->type() == QEvent::LanguageChange)
	{
		ui->retranslateUi(this);
		updateNoCard();
		updateTabs();
		updatePinControls();
	}
	QDialog::changeEvent(event);
}

void SigningDialog::updateNoCard()
{
	if(!QPCSC::instance().serviceRunning())
		ui->noCardStatus->setText(tr("The PCSC service, required for using the ID-card, is not working. Check your computer settings."));
	else if(QPCSC::instance().readers().isEmpty())
		ui->noCardStatus->setText(tr("No readers found"));
	else
		ui->noCardStatus->setText(tr("No card in reader"));
	ui->noCard->setAccessibleDescription(ui->noCardStatus->text());
}

void SigningDialog::reject()
{
	if(ui->progressPage->isVisible())
	{
		if(ui->progressCancel->isVisible() && ui->progressCancel->isEnabled())
			ui->progressCancel->click();
		return;
	}
	QDialog::reject();
}

void SigningDialog::updateTabs()
{
	ui->tabs->setTabText(IDCard, tr("ID-card"));
	ui->tabs->setTabText(MobileID, tr("Mobile-ID"));
	ui->tabs->setTabText(SmartID, tr("Smart-ID"));
}

void SigningDialog::updateCards(QList<TokenData> tokens)
{
	qDeleteAll(ui->cardListContainer->findChildren<CardListItem*>());

	bool any = false;
	CardListItem *selectedCard = nullptr;
	for(const TokenData &token : tokens) {
		SslCertificate c(token.cert());
		if(!c.keyUsage().contains(SslCertificate::NonRepudiation))
			continue;
		auto *w = new CardListItem(ui->cardListContainer);
		w->update(token, token == m_selectedToken);
		connect(w, &CardListItem::selected, this, &SigningDialog::updateSelection);
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
	updateSelection();
}

void SigningDialog::updateSelection()
{
	for(auto *w: ui->cardListContainer->findChildren<CardListItem*>()) {
		if(!w->isChecked())
			continue;
		if(!(w->token() == m_selectedToken)) {
			ui->pin->clear();
			setError(ui->pin, ui->errorPin, {});
		}
		if(currentMethod() == IDCard)
			w->refresh();
		m_selectedToken = w->token();
		break;
	}
	updatePinControls();
}

void SigningDialog::updatePinControls()
{
	bool usable = std::ranges::any_of(ui->cardListContainer->findChildren<CardListItem*>(),
		[](CardListItem *w) { return w->isChecked() && w->isEnabled(); });
	bool isSeal = SslCertificate(m_selectedToken.cert()).type() & SslCertificate::TempelType;
	QString pinName = isSeal ? QStringLiteral("PIN") : QSmartCardData::typeString(QSmartCardData::Pin2Type);
	bool showPin = false;
#ifndef Q_OS_WIN
	if(usable && !m_selectedToken.data(QStringLiteral("pinpad")).toBool()) {
		showPin = true;
		int minLen = m_selectedToken.data(QStringLiteral("pinMin")).toInt();
		int maxLen = m_selectedToken.data(QStringLiteral("pinMax")).toInt();
		ui->pin->setPinLen(std::max(minLen, 5), std::max(maxLen, 12), !isSeal);
		ui->labelPin->setText(tr("Enter %1 code").arg(pinName));
	}
#endif
	ui->labelPin->setVisible(showPin);
	ui->pin->setVisible(showPin);
	ui->errorPin->setVisible(showPin && !ui->errorPin->text().isEmpty());
	ui->layoutPin->invalidate();
	if(!showPin)
		ui->pin->clear();

	ui->infoPin->setVisible(usable && !showPin);
	if(usable && !showPin)
		ui->infoPin->setText(tr("Please enter %1 code in the new window that will appear shortly").arg(pinName));
	if(currentMethod() == IDCard)
		ui->sign->setEnabled(usable && (!showPin || ui->pin->hasAcceptableInput()));
}

void SigningDialog::performSign()
{
	auto showPage = [this](QWidget *page)
	{
		ui->formPage->setVisible(page == ui->formPage);
		ui->progressPage->setVisible(page == ui->progressPage);
	};

	switch(currentMethod()) {
	case IDCard: {
		if(!confirmAlreadySigned())
			return reject();
		QString pin = ui->pin->isVisible() ? ui->pin->text() : QString();
		auto val = QCryptoBackend::getBackend(m_selectedToken, pin);
		if(!val) {
			switch(val.error()) {
			case QCryptoBackend::PinIncorrect: {
				QString msg = QCryptoBackend::errorString(QCryptoBackend::PinIncorrect);
				if(QSmartCardData data = QSmartCard::readCard(m_selectedToken, true); !data.isNull())
					msg += QStringLiteral(". ") + tr("Remaining attempts: %1").arg(data.retryCount(QSmartCardData::Pin2Type));
				ui->pin->clear();
				setError(ui->pin, ui->errorPin, msg);
				ui->pin->setFocus();
				ui->sign->setEnabled(false);
				return;
			}
			case QCryptoBackend::PinCanceled:
				return reject();
			default:
				WarningDialog::create(this)
					->withTitle(tr("Failed to sign document"))
					->withText(QCryptoBackend::errorString(val.error()))
					->exec();
				return reject();
			}
		}
		QSigner signer(std::unique_ptr<QCryptoBackend>(val.value()), m_selectedToken);
		return digiDoc->sign(m_city, m_state, m_zip, m_country, m_role, &signer) ? accept() : reject();
	}
	case MobileID: {
		static const QStringList countryCodes {QStringLiteral("372"), QStringLiteral("370")};
		const QString code = ui->idCodeMID->text();
		const QString phone = ui->phoneNo->text();
		setError(ui->idCodeMID, ui->errorCodeMID, IKValidator::isValid(code) ? QString() : tr("Personal code is not valid"));
		if(phone.size() < 8 || countryCodes.contains(phone))
			setError(ui->phoneNo, ui->errorPhone, tr("Phone number is not entered"));
		else if(!countryCodes.contains(phone.left(3)))
			setError(ui->phoneNo, ui->errorPhone, tr("Invalid country code"));
		else
			setError(ui->phoneNo, ui->errorPhone, {});
		if(!ui->errorCodeMID->text().isEmpty() || !ui->errorPhone->text().isEmpty())
			return;
		if(!confirmAlreadySigned())
			return reject();
		SigningProgressUI pui { ui->progressInfo, ui->progressCode, ui->progressLabel, ui->progressBar, ui->progressCancel, this };
		showPage(ui->progressPage);
		MobileProgress m(pui);
		return m.init(code, phone) &&
			digiDoc->sign(m_city, m_state, m_zip, m_country, m_role, &m) ? accept() : QDialog::reject();
	}
	case SmartID: {
		const QString code = ui->idCodeSID->text();
		if(ui->idCodeSID->validator() && !IKValidator::isValid(code)) {
			setError(ui->idCodeSID, ui->errorCodeSID, tr("Personal code is not valid"));
			return;
		}
		setError(ui->idCodeSID, ui->errorCodeSID, {});
		if(!confirmAlreadySigned())
			return reject();
		SigningProgressUI pui { ui->progressInfo, ui->progressCode, ui->progressLabel, ui->progressBar, ui->progressCancel, this };
		showPage(ui->progressPage);
		SmartIDProgress s(pui);
		return s.init(ui->idCountry->currentData().toString(), code, digiDoc->fileName()) &&
			digiDoc->sign(m_city, m_state, m_zip, m_country, m_role, &s) ? accept() : QDialog::reject();
	}
	}
}
