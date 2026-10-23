// SPDX-FileCopyrightText: Estonian Information System Authority
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "PinpadPopup.h"
#include "ui_PinpadPopup.h"
#include "SslCertificate.h"
#include "effects/Overlay.h"

#include <QSvgWidget>

PinpadPopup::PinpadPopup(QSmartCardData::PinType type, QSmartCard::PinAction action,
	const QSmartCardData &current, QWidget *parent)
	: QDialog(parent)
{
	Ui::PinpadPopup ui {};
	ui.setupUi(this);
	setWindowFlags(Qt::Dialog | Qt::CustomizeWindowHint);
	new Overlay(this);
	connect(this, &PinpadPopup::finished, this, &PinpadPopup::close);

	QString bodyText;
	switch(action)
	{
	case QSmartCard::ActivateWithPuk:
	case QSmartCard::ChangeWithPuk:
		bodyText = tr("To change %1 code with the PUK code on a PinPad reader the PUK code has to be entered first and then the %1 code twice.").arg(QSmartCardData::typeString(type));
		break;
	case QSmartCard::UnblockWithPuk:
		bodyText = tr("To unblock the %1 code on a PinPad reader the PUK code has to be entered first and then the %1 code twice.").arg(QSmartCardData::typeString(type));
		break;
	default:
		bodyText = tr("To change %1 on a PinPad reader the old %1 code has to be entered first and then the new %1 code twice.").arg(QSmartCardData::typeString(type));
		break;
	}

	const SslCertificate c = current.authCert();
	ui.label->setText(c.toString(c.showCN() ? QStringLiteral("CN, serialNumber") : QStringLiteral("GN SN, serialNumber")));
	ui.text->setText(bodyText);
	const auto src = action == QSmartCard::ChangeWithPin || action == QSmartCard::ActivateWithPin ?
		type : QSmartCardData::PukType;
	const auto retryCount = current.retryCount(src);
	if(retryCount < 3)
		ui.errorPin->setText((src == QSmartCardData::PukType ?
			tr("PUK remaining attempts: %1") : tr("Remaining attempts: %1")).arg(retryCount));
	else
		ui.errorPin->hide();
	auto *movie = new QSvgWidget(QStringLiteral(":/images/wait.svg"), this);
	movie->setFixedSize(30, 30);
	movie->show();
	ui.layoutContent->addWidget(movie, 0, Qt::AlignCenter);
}

void PinpadPopup::reject()
{
	// The entry runs on the reader and ends there, not with Esc
}
