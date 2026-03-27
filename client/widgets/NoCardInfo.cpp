// SPDX-FileCopyrightText: Estonian Information System Authority
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "NoCardInfo.h"
#include "ui_NoCardInfo.h"

NoCardInfo::NoCardInfo( QWidget *parent )
	: QWidget(parent)
	, ui(new Ui::NoCardInfo)
{
	ui->setupUi( this );
	ui->cardIcon->load(QStringLiteral(":/images/icon_IDkaart_red.svg"));
}

NoCardInfo::~NoCardInfo()
{
	delete ui;
}

void NoCardInfo::update(Status s)
{
	status = s;
	switch(status)
	{
	case NoPCSC:
		ui->cardStatus->setText(tr("The PCSC service, required for using the ID-card, is not working. Check your computer settings."));
		break;
	case NoReader:
		ui->cardStatus->setText(tr("No readers found"));
		break;
	case Loading:
		ui->cardStatus->setText(tr("Loading data"));
		break;
	default:
		ui->cardStatus->setText(tr("No card in reader"));
		break;
	}
	setAccessibleDescription(ui->cardStatus->text());
}

void NoCardInfo::changeEvent(QEvent* event)
{
	if (event->type() == QEvent::LanguageChange)
		update(status);

	QWidget::changeEvent(event);
}
