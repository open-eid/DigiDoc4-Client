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

#include "MyEidInfo.h"
#include "ui_MyEidInfo.h"

#include "Application.h"
#include "CardPopup.h"
#include "QCryptoBackend.h"
#include "QPCSC.h"
#include "SslCertificate.h"

#include <QDateTime>
#include <QMouseEvent>
#include <QShowEvent>
#include <QStyle>

#include <algorithm>
#include <functional>

MyEidInfo::MyEidInfo( QWidget *parent )
	: QWidget(parent)
	, ui(new Ui::MyEidInfo)
{
	ui->setupUi( this );
	connect(ui->authBox, &VerifyCert::changePinClicked, this, [this](QSmartCard::PinAction action) {
		changePin(QSmartCardData::Pin1Type, action);
	});
	connect(ui->signBox, &VerifyCert::changePinClicked, this,[this](QSmartCard::PinAction action) {
		changePin(QSmartCardData::Pin2Type, action);
	});
	connect(ui->pukBox, &VerifyCert::changePinClicked, this, [this](QSmartCard::PinAction action) {
		changePin(QSmartCardData::PukType, action);
	});
	updateSelector();
	updateData();
	connect(ui->cardInfo, &CardWidget::selected, ui->selector, &QToolButton::toggle);
	connect(ui->selector, &QToolButton::toggled, this, [this](bool show) {
		if(show)
		{
			const auto list = otherCards();
			if(list.isEmpty())
			{
				ui->selector->setChecked(false);
				return;
			}
			cardPopup = new CardPopup(list, this);
			qApp->installEventFilter(this);
			connect(cardPopup, &CardPopup::activated, this, [this](const TokenData &token) {
				ui->selector->setChecked(false);
				selectCard(token);
			});
			cardPopup->show();
		}
		else if(cardPopup)
		{
			qApp->removeEventFilter(this);
			cardPopup->hide();
			cardPopup->deleteLater();
			cardPopup = nullptr;
		}
	});
	auto *manager = qApp->cryptoManager();
	connect(manager, &QCryptoManager::cacheChanged, this, &MyEidInfo::updateCards);
	connect(manager, &QCryptoManager::refreshed, this, [this] {
		updateEmptyState(true);
	});
	connect(manager, &QCryptoManager::cardDataReady, this, [this](quint64 id, const QSmartCardData &data) {
		if(id != request)
			return;
		request = 0;
		currentData = data;
		readFailed = currentData.isNull();
		updateData();
		Q_EMIT dataChanged(currentData);
	});
	connect(manager, &QCryptoManager::tokenChanged, this, [this](const TokenData &token) {
		if(!selectedToken.isNull() && token.card() == selectedToken.card() && token.reader() == selectedToken.reader())
			refresh();
	});
	updateCards(manager->cache());
}

MyEidInfo::~MyEidInfo()
{
	delete ui;
}

QSmartCardData MyEidInfo::cardData() const { return currentData; }

void MyEidInfo::changePin(QSmartCardData::PinType type, QSmartCard::PinAction action)
{
	QSmartCard::pinChange(selectedToken, currentData, type, action, parentWidget());
}

void MyEidInfo::updateCards(QList<TokenData> tokens)
{
	tokens.removeIf(std::not_fn([](const TokenData &token) {
		return SslCertificate(token.cert()).type() &
			(SslCertificate::EstEidType|SslCertificate::DigiIDType|SslCertificate::TempelType);
	}));
	cards = std::move(tokens);
	if(cards.contains(selectedToken))
	{
		updateSelector();
		return;
	}
	auto selected = std::find_if(cards.cbegin(), cards.cend(), [this](const TokenData &token) {
		return token.card() == selectedToken.card() && token.reader() == selectedToken.reader();
	});
	selectCard(selected == cards.cend() ? cards.value(0) : *selected);
}

void MyEidInfo::selectCard(const TokenData &token)
{
	if(token == selectedToken)
		return;
	selectedToken = token;
	currentData = {};
	readFailed = false;
	request = 0;
	updateSelector();
	updateData();
	Q_EMIT dataChanged(currentData);
	refresh();
}

void MyEidInfo::refresh()
{
	if(request || selectedToken.isNull() || selectedToken.reader().isEmpty())
		return;
	request = qApp->cryptoManager()->readCard(selectedToken, currentData);
}

void MyEidInfo::changeEvent(QEvent* event)
{
	if (event->type() == QEvent::LanguageChange)
	{
		ui->retranslateUi(this);
		updateLabels();
		updateEmptyState();
	}

	QWidget::changeEvent(event);
}

void MyEidInfo::updateLabels()
{
	const SslCertificate cert = currentData.isNull() ? SslCertificate(selectedToken.cert()) : currentData.authCert();
	const auto certType = cert.type();
	const QDateTime expiry = currentData.isNull() ? cert.expiryDate() : currentData.data(QSmartCardData::Expiry).toDateTime();
	ui->valueExpiryDate->setText(expiry.toString(QStringLiteral("dd.MM.yyyy")));
	if(certType & SslCertificate::DigiIDType)
	{
		ui->valueExpiryDate->setLabel({});
		ui->valueExpiryDate->setText(tr("You're using digital identity card"));
	} else if(expiry < QDateTime::currentDateTime())
		ui->valueExpiryDate->setLabel(QStringLiteral("error"));
	else
		ui->valueExpiryDate->setLabel(QStringLiteral("good"));

	ui->labelDocument->setHidden(certType & SslCertificate::TempelType);
	ui->valueCitizenship->setHidden(ui->valueCitizenship->text().isEmpty());
	ui->labelCitizenship->setHidden(ui->valueCitizenship->text().isEmpty());
	if(certType & SslCertificate::TempelType)
	{
		ui->labelGivenNames->setText(tr("Name"));
		ui->labelSurname->setText(tr("Organization"));
		ui->labelPersonalCode->setText(tr("Serial"));
		ui->labelCitizenship->setText(tr("Country"));
	}
	else
	{
		ui->labelGivenNames->setText(tr("Given names"));
		ui->labelSurname->setText(tr("Surname"));
		ui->labelPersonalCode->setText(tr("Personal code"));
		ui->labelCitizenship->setText(tr("Citizenship"));
	}
}

void MyEidInfo::updateData()
{
	bool waiting = currentData.isNull() &&
		!(SslCertificate(selectedToken.cert()).type() & SslCertificate::TempelType);
	ui->cardDetails->setHidden(waiting);
	ui->noReaderInfo->setVisible(waiting);
	updateEmptyState();

	if(currentData.isNull())
	{
		ui->valueGivenNames->clear();
		ui->valueSurname->clear();
		ui->valuePersonalCode->clear();
		ui->valueCitizenship->clear();
		ui->valueExpiryDate->clear();
		ui->valueDocument->clear();
		ui->authBox->clear();
		ui->signBox->clear();
		ui->pukBox->clear();
		SslCertificate cert(selectedToken.cert());
		if(!(cert.type() & SslCertificate::TempelType))
			return;
		bool isSign = cert.keyUsage().contains(SslCertificate::NonRepudiation);
		ui->valueGivenNames->setText(cert.toString(QStringLiteral("CN")));
		ui->valueSurname->setText(cert.toString(QStringLiteral("O")));
		ui->valuePersonalCode->setText(cert.personalCode());
		ui->valueCitizenship->setText(cert.toString(QStringLiteral("C")));
		ui->authBox->setHidden(isSign);
		ui->signBox->setVisible(isSign);
		if(isSign)
			ui->signBox->update(QSmartCardData::Pin2Type, cert);
		else
			ui->authBox->update(QSmartCardData::Pin1Type, cert);
		ui->pukBox->hide();
	}
	else
	{
		ui->valueGivenNames->setText(currentData.data(QSmartCardData::FirstName).toString());
		ui->valueSurname->setText(currentData.data(QSmartCardData::SurName).toString());
		ui->valuePersonalCode->setText(currentData.data(QSmartCardData::Id).toString());
		ui->valueCitizenship->setText(currentData.data(QSmartCardData::Citizen).toString());
		ui->valueDocument->setText(currentData.data(QSmartCardData::DocumentId).toString());

		ui->authBox->setVisible(!currentData.authCert().isNull());
		if(!currentData.authCert().isNull())
			ui->authBox->update(QSmartCardData::Pin1Type, currentData);

		ui->signBox->setVisible(!currentData.signCert().isNull());
		if(!currentData.signCert().isNull())
			ui->signBox->update(QSmartCardData::Pin2Type, currentData);

		ui->pukBox->show();
		ui->pukBox->update(QSmartCardData::PukType, currentData);
	}

	updateLabels();
}

void MyEidInfo::hideEvent(QHideEvent *event)
{
	ui->selector->setChecked(false);
	QWidget::hideEvent(event);
}

bool MyEidInfo::eventFilter(QObject *watched, QEvent *event)
{
	if(cardPopup && event->type() == QEvent::MouseButtonPress)
	{
		auto *widget = qobject_cast<QWidget*>(watched);
		auto pos = static_cast<QMouseEvent*>(event)->globalPosition().toPoint();
		auto contains = [pos](QWidget *item) {
			return item->isVisible() && item->rect().contains(item->mapFromGlobal(pos));
		};
		if(widget && widget->window() == window() &&
			!contains(cardPopup) && !contains(ui->cardInfo) && !contains(ui->selector))
			ui->selector->setChecked(false);
	}
	return QWidget::eventFilter(watched, event);
}

void MyEidInfo::showEvent(QShowEvent *event)
{
	if(!event->spontaneous())
		refresh();
	QWidget::showEvent(event);
}

void MyEidInfo::updateEmptyState(bool enumerationComplete)
{
	if(ui->noReaderInfo->isHidden())
		return;
	if(!selectedToken.isNull())
		emptyStateText = readFailed ? QT_TR_NOOP("Failed to read card data") : QT_TR_NOOP("Reading card...");
	else if(enumerationComplete)
	{
		emptyStateText = QT_TR_NOOP("The card in the reader is not supported. Insert an Estonian ID-card.");
		auto &pcsc = QPCSC::instance();
		const QStringList readers = pcsc.readers();
		if(!pcsc.serviceRunning())
			emptyStateText = QT_TR_NOOP("The PCSC service, required for using the ID-card, is not working. Check your computer settings.");
		else if(readers.isEmpty())
			emptyStateText = QT_TR_NOOP("No card reader found. Connect a card reader to your computer.");
		else if(std::none_of(readers.cbegin(), readers.cend(), [](const QString &reader) {
			return QPCSCReader(reader).isPresent();
		}))
			emptyStateText = QT_TR_NOOP("No card in the reader. Insert your ID-card into the reader.");
	}
	ui->noReaderInfoText->setText(tr(emptyStateText));
}

QList<TokenData> MyEidInfo::otherCards() const
{
	QList<TokenData> list;
	for(const TokenData &token: cards)
	{
		if(token.card() == selectedToken.card())
			continue;
		if(std::any_of(list.cbegin(), list.cend(), [token](const TokenData &item) { return token.card() == item.card(); }))
			continue;
		list.append(token);
	}
	return list;
}

void MyEidInfo::updateSelector()
{
	ui->topBar->setVisible(!selectedToken.isNull());
	const auto count = otherCards().size();
	ui->selector->setHidden(count == 0);
	ui->selector->setChecked(false);
	ui->cardInfo->setCursor(count > 0 ? Qt::PointingHandCursor : Qt::ArrowCursor);
	ui->cardInfo->update(selectedToken, count > 1);
}
