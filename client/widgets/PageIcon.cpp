// SPDX-FileCopyrightText: Estonian Information System Authority
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "PageIcon.h"

#include <QSvgWidget>

PageIcon::PageIcon(QWidget *parent)
	: QToolButton(parent)
	, errorIcon(new QSvgWidget(this))
{
	errorIcon->resize(22, 22);
	errorIcon->move(64, 6);
	errorIcon->hide();
}

void PageIcon::invalidIcon(bool show)
{
	err = show;
	updateIcon();
}

void PageIcon::updateIcon()
{
	if(err)
		errorIcon->load(QStringLiteral(":/images/icon_alert_error.svg"));
	else if(warn)
		errorIcon->load(QStringLiteral(":/images/icon_alert_warning.svg"));
	errorIcon->setVisible(err || warn);
}

void PageIcon::warningIcon(bool show)
{
	warn = show;
	updateIcon();
}
