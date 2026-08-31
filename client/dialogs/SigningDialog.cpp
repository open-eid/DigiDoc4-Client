// SPDX-FileCopyrightText: Estonian Information System Authority
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "SigningDialog.h"
#include "ui_SigningDialog.h"

#include "Application.h"
#include "DigiDoc.h"
#include "IKValidator.h"
#include "QPCSC.h"
#include "QCryptoBackend.h"
#include "Settings.h"
#include "SslCertificate.h"
#include "dialogs/WarningDialog.h"
#include "effects/Overlay.h"
#include "signer/QSigner.h"
#include "signer/MobileProgress.h"
#include "signer/SmartIDProgress.h"
#include "widgets/CardListItem.h"
#include "widgets/NoCardInfo.h"

#include <QtWidgets/QMessageBox>

#include <memory>

SigningDialog::SigningDialog(DigiDoc *doc, QWidget *parent)
	: QDialog(parent)
	, digiDoc(doc)
	, ui(new Ui::SigningDialog)
{
	static const QStringList countryCodes {QStringLiteral("372"), QStringLiteral("370")};
	new Overlay(this);
	ui->setupUi(this);
	ui->progressPage->hide();
	setWindowFlags(Qt::Dialog | Qt::CustomizeWindowHint);
#ifdef Q_OS_WIN
	ui->buttonLayout->setDirection(QBoxLayout::RightToLeft);
#endif

	// Tab bar
	ui->tabs->addTab(tr("ID-card"));
	ui->tabs->addTab(tr("Mobile-ID"));
	ui->tabs->addTab(tr("Smart-ID"));
	connect(ui->tabs, &QTabBar::currentChanged, ui->tabStack, &QStackedWidget::setCurrentIndex);

	// ID-card tab
	updateCards();
	connect(qApp->cryptoManager(), &QCryptoManager::cacheChanged, this, &SigningDialog::updateCards);
	ui->pin->setPinLen(5); // PIN2 (signing) is 5..12 digits
	connect(ui->pin, &QLineEdit::textEdited, this, [this] {
		if(!ui->errorPin->text().isEmpty()) {
			ui->errorPin->clear();
			ui->errorPin->setVisible(false);
			ui->pin->setLabel({});
		}
		ui->sign->setEnabled(ui->pin->hasAcceptableInput());
	});
	connect(ui->pin, &QLineEdit::returnPressed, ui->sign, &QPushButton::click);
	connect(ui->tabs, &QTabBar::currentChanged, this, [this](int index) {
		if(index == IDCard)
			updateCards();
		else
			ui->sign->setEnabled(true);
	});

	auto setError = [](LineEdit *input, QLabel *error, const QString &msg) {
		input->setLabel(msg.isEmpty() ? QString() : QStringLiteral("error"));
		error->setText(msg);
		error->setHidden(msg.isEmpty());
	};

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
	connect(ui->idCodeMID, &QLineEdit::textEdited, this, [this, setError] {
		setError(ui->idCodeMID, ui->errorCodeMID, {});
	});
	connect(ui->phoneNo, &QLineEdit::returnPressed, ui->sign, &QPushButton::click);
	connect(ui->phoneNo, &QLineEdit::textEdited, this, saveMIDSettings);
	connect(ui->phoneNo, &QLineEdit::textEdited, this, [this, setError] {
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
	connect(ui->idCodeSID, &QLineEdit::textEdited, this, [this, setError] {
		setError(ui->idCodeSID, ui->errorCodeSID, {});
	});
	connect(ui->idCountry, &QComboBox::currentTextChanged, this, [this, ik, saveSIDSettings] {
		static const QString &EE2 = Settings::SMARTID_COUNTRY_LIST.first();
		ui->idCodeSID->setValidator(ui->idCountry->currentData().toString() == EE2 ? ik : nullptr);
		saveSIDSettings();
	});
	connect(ui->cbRememberSID, &QCheckBox::clicked, this, saveSIDSettings);

	// Sign button — validates per-tab then delegates to performSign()
	connect(ui->sign, &QPushButton::clicked, this, [this, setError] {
		switch(currentMethod()) {
		case IDCard:
			if(performSign()) accept();
			else reject();
			break;
		case MobileID: {
			const QString code = ui->idCodeMID->text();
			const QString phone = ui->phoneNo->text();
			if(!IKValidator::isValid(code))
				setError(ui->idCodeMID, ui->errorCodeMID, tr("Personal code is not valid"));
			else
				setError(ui->idCodeMID, ui->errorCodeMID, {});
			if(phone.size() < 8 || countryCodes.contains(phone))
				setError(ui->phoneNo, ui->errorPhone, tr("Phone number is not entered"));
			else if(!countryCodes.contains(phone.left(3)))
				setError(ui->phoneNo, ui->errorPhone, tr("Invalid country code"));
			else
				setError(ui->phoneNo, ui->errorPhone, {});
			if(ui->errorCodeMID->text().isEmpty() && ui->errorPhone->text().isEmpty()) {
				if(performSign()) accept();
				else reject();
			}
			break;
		}
		case SmartID:
			if(ui->idCodeSID->validator() && !IKValidator::isValid(ui->idCodeSID->text()))
				setError(ui->idCodeSID, ui->errorCodeSID, tr("Personal code is not valid"));
			else {
				setError(ui->idCodeSID, ui->errorCodeSID, {});
				if(performSign()) accept();
				else reject();
			}
			break;
		}
	});
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

QString SigningDialog::currentCode() const
{
	switch(currentMethod()) {
	case IDCard:
		return SslCertificate(m_selectedToken.cert()).personalCode();
	case MobileID:
		return ui->idCodeMID->text();
	case SmartID:
		return ui->idCodeSID->text();
	}
	return {};
}

void SigningDialog::updateCards()
{
	qDeleteAll(ui->cardListContainer->findChildren<CardListItem*>());

	// Fall back to global selection if our local token was removed
	const QList<TokenData> tokens = qApp->cryptoManager()->cache();
	bool localValid = std::any_of(tokens.cbegin(), tokens.cend(),
		[this](const TokenData &t) { return t == m_selectedToken; });
	if(!localValid)
		m_selectedToken = qApp->cryptoManager()->tokensign();

	bool any = false;
	bool selectedUsable = false;
	for(const TokenData &token : tokens) {
		SslCertificate c(token.cert());
		if(!c.keyUsage().contains(SslCertificate::NonRepudiation))
			continue;
		bool usable = c.isValid() && !token.data(QStringLiteral("blocked")).toBool();
		auto *w = new CardListItem(ui->cardListContainer);
		w->update(token, token.card() == m_selectedToken.card(), usable);
		connect(w, &CardListItem::selected, this, [this](const TokenData &t) {
			m_selectedToken = t;
			updateCards();
		});
		ui->cardListContainer->layout()->addWidget(w);
		any = true;
		if(token == m_selectedToken)
			selectedUsable = usable;
	}

	if(!any) {
		if(!QPCSC::instance().serviceRunning())
			ui->noCard->update(NoCardInfo::NoPCSC);
		else if(QPCSC::instance().readers().isEmpty())
			ui->noCard->update(NoCardInfo::NoReader);
		else
			ui->noCard->update(NoCardInfo::NoCard);
	}
	ui->noCard->setVisible(!any);
	ui->cardListContainer->setVisible(any);

	// The inline PIN2 field applies only to software PIN entry: PKCS11 readers
	// (non-Windows) that are not PinPads. PinPad and Windows keep their own PIN UI
	// (hardware / OS dialog) via the empty-pin path in performSign().
	bool showPin = false;
#ifndef Q_OS_WIN
	showPin = selectedUsable && !m_selectedToken.data(QStringLiteral("pinpad")).toBool();
#endif
	ui->labelPin->setVisible(showPin);
	ui->pin->setVisible(showPin);
	ui->errorPin->setVisible(showPin && !ui->errorPin->text().isEmpty());
	if(!showPin)
		ui->pin->clear();

	if(currentMethod() == IDCard)
		ui->sign->setEnabled(selectedUsable && (!showPin || ui->pin->hasAcceptableInput()));
}

bool SigningDialog::performSign()
{
	// Warn if this personal code has already signed the document
	const QString code = currentCode();
	if(!code.isEmpty()) {
		const bool alreadySigned = std::any_of(
			digiDoc->signatures().cbegin(), digiDoc->signatures().cend(),
			[&code](const DigiDocSignature &sig) {
				return SslCertificate(sig.cert()).personalCode() == code;
			});
		if(alreadySigned) {
			auto *dlg = WarningDialog::create(this)
				->withTitle(tr("The document has already been signed by you"))
				->addButton(tr("Continue signing"), QMessageBox::Ok);
			if(dlg->exec() != QMessageBox::Ok)
				return false;
		}
	}

	auto showPage = [this](QWidget *page)
	{
		ui->formPage->setVisible(page == ui->formPage);
		ui->progressPage->setVisible(page == ui->progressPage);
	};

	switch(currentMethod()) {
	case IDCard: {
		// Software PIN entry uses the inline field; PinPad/Windows pass an empty
		// PIN so login() falls back to the hardware/OS prompt.
		QString pin = ui->pin->isVisible() ? ui->pin->text() : QString();
		auto val = QCryptoBackend::getBackend(m_selectedToken, pin);
		if(!val) {
			switch(val.error()) {
			case QCryptoBackend::PinIncorrect:
				// Show the error under the field and let the user retype + re-sign.
				ui->pin->setLabel(QStringLiteral("error"));
				ui->errorPin->setText(QCryptoBackend::errorString(QCryptoBackend::PinIncorrect));
				ui->errorPin->setVisible(true);
				ui->pin->clear();
				ui->pin->setFocus();
				ui->sign->setEnabled(false);
				return false;
			case QCryptoBackend::PinCanceled:
				return false;
			default:
				WarningDialog::create(this)
					->withTitle(tr("Failed to sign document"))
					->withText(QCryptoBackend::errorString(val.error()))
					->exec();
				return false;
			}
		}
		std::unique_ptr<QCryptoBackend> backend(val.value());
		QSigner signer(backend.get(), m_selectedToken);
		return digiDoc->sign(m_city, m_state, m_zip, m_country, m_role, &signer);
	}
	case MobileID: {
		SigningProgressUI pui { ui->progressInfo, ui->progressCode, ui->progressLabel, ui->progressBar, ui->progressCancel, this };
		showPage(ui->progressPage);
		MobileProgress m(pui);
		return m.init(ui->idCodeMID->text(), ui->phoneNo->text()) &&
			digiDoc->sign(m_city, m_state, m_zip, m_country, m_role, &m);
	}
	case SmartID: {
		SigningProgressUI pui { ui->progressInfo, ui->progressCode, ui->progressLabel, ui->progressBar, ui->progressCancel, this };
		showPage(ui->progressPage);
		SmartIDProgress s(pui);
		return s.init(ui->idCountry->currentData().toString(), ui->idCodeSID->text(), digiDoc->fileName()) &&
			digiDoc->sign(m_city, m_state, m_zip, m_country, m_role, &s);
	}
	}
	return false;
}
