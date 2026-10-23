// SPDX-FileCopyrightText: Estonian Information System Authority
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "QCryptoBackend.h"
#include "SslCertificate.h"
#include "widgets/CardListItem.h"

#include <QtCore/QTimeLine>
#include <QtWidgets/QLabel>
#include <QtWidgets/QProgressBar>
#include <QtWidgets/QPushButton>

#include <algorithm>

class PinLogin
{
	Q_DECLARE_TR_FUNCTIONS(PinLogin)
public:
	template<class Ui>
	static void updateSelection(Ui *ui, TokenData &token, bool refresh = true)
	{
		for(auto *w: ui->cardListContainer->template findChildren<CardListItem*>()) {
			if(!w->isChecked())
				continue;
			if(!(w->token() == token)) {
				ui->pin->clear();
				clearError(ui);
			}
			if(refresh)
				w->refresh();
			token = w->token();
			break;
		}
	}

	template<class Ui>
	static bool update(Ui *ui, const TokenData &token)
	{
		const bool usable = std::ranges::any_of(ui->cardListContainer->template findChildren<CardListItem*>(),
			[](CardListItem *w) { return w->isChecked() && w->isEnabled(); });
		bool showPin = false;
#ifndef Q_OS_WIN
		showPin = usable && !token.data(QStringLiteral("pinpad")).toBool();
#endif
		if(showPin)
		{
			const SslCertificate cert(token.cert());
			const bool isSeal = cert.type() & SslCertificate::TempelType;
			const int minLen = std::max(token.data(QStringLiteral("pinMin")).toInt(),
				int(QSmartCardData::minPinLen(QSmartCardData::pinType(cert))));
			const int maxLen = std::max(token.data(QStringLiteral("pinMax")).toInt(), 12);
			ui->pin->setPinLen(minLen, maxLen, !isSeal);
			ui->labelPin->setText(tr("Enter %1 code").arg(pinName(token)));
		}
		ui->labelPin->setVisible(showPin);
		ui->pin->setVisible(showPin);
		ui->errorPin->setVisible(usable && !ui->errorPin->text().isEmpty());
		ui->layoutPin->invalidate();
		if(!showPin)
			ui->pin->clear();
		ui->infoPin->setVisible(usable && !showPin);
		if(usable && !showPin)
			ui->infoPin->setText(hint(token));
		return usable && (!showPin || ui->pin->hasAcceptableInput());
	}

	template<class Ui>
	static void clearError(Ui *ui)
	{
		ui->pin->setLabel({});
		ui->errorPin->clear();
		ui->errorPin->hide();
	}

	template<class Ui>
	static std::expected<QCryptoBackend*,QCryptoBackend::Status> login(Ui *ui, const TokenData &token)
	{
		const QString pin = ui->pin->isVisible() ? ui->pin->text() : QString();
		const bool pinpad = pin.isEmpty() && token.data(QStringLiteral("pinpad")).toBool();
		if(pinpad)
			clearError(ui);
		auto result = pinpad ? loginOnPinpad(ui, token) : QCryptoBackend::getBackend(token, pin);
		if(!result && result.error() == QCryptoBackend::PinIncorrect)
		{
			ui->pin->clear();
			ui->pin->setLabel(QStringLiteral("error"));
			ui->errorPin->setText(QCryptoBackend::errorString(result.error(), token));
			ui->errorPin->show();
			if(ui->pin->isVisible())
				ui->pin->setFocus();
		}
		return result;
	}

private:
	static QString hint(const TokenData &token)
	{
		return (token.data(QStringLiteral("pinpad")).toBool() ?
			tr("After you continue, enter %1 code on the card reader keypad") :
			tr("Please enter %1 code in the new window that will appear shortly")).arg(pinName(token));
	}

	template<class Ui>
	static std::expected<QCryptoBackend*,QCryptoBackend::Status> loginOnPinpad(Ui *ui, const TokenData &token)
	{
		const QString name = pinName(token);
		QString text = tr("Enter %1 code on the card reader keypad").arg(name);
		if(token.data(QStringLiteral("pinFinalTry")).toBool())
			text += QLatin1Char('\n') + tr("%1 will be locked next failed attempt").arg(name);
		else if(token.data(QStringLiteral("pinCountLow")).toBool())
			text += QLatin1Char('\n') + tr("%1 has been entered incorrectly at least once").arg(name);
		ui->progressInfo->setText(text);
		for(QWidget *w: {(QWidget*)ui->progressLabel, (QWidget*)ui->progressCode, (QWidget*)ui->progressCancel})
			w->hide();
		constexpr int seconds = 30;
		ui->progressBar->setRange(0, seconds);
		ui->progressBar->setValue(seconds);
		QTimeLine countdown(seconds * 1000);
		countdown.setEasingCurve(QEasingCurve::Linear);
		countdown.setFrameRange(seconds, 0);
		QObject::connect(&countdown, &QTimeLine::frameChanged, ui->progressBar, &QProgressBar::setValue);
		ui->formPage->hide();
		ui->progressPage->show();
		countdown.start();

		auto result = QCryptoBackend::getBackend(token);

		ui->progressPage->hide();
		ui->formPage->show();
		for(QWidget *w: {(QWidget*)ui->progressLabel, (QWidget*)ui->progressCode, (QWidget*)ui->progressCancel})
			w->show();
		return result;
	}

	static QString pinName(const TokenData &token)
	{
		SslCertificate cert(token.cert());
		return cert.type() & SslCertificate::TempelType ? QStringLiteral("PIN") :
			QSmartCardData::typeString(QSmartCardData::pinType(cert));
	}
};
