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

#include "WarningRibbon.h"
#include "ui_WarningRibbon.h"

#include "Styles.h"

class WarningRibbon::Private: public Ui::WarningRibbon
{
public:
	int count = 0;
	bool expanded = false;
};

WarningRibbon::WarningRibbon(int count, QWidget *parent)
	: StyledWidget(parent)
	, ui(new Private)
{
	ui->setupUi(this);
	ui->details->setFont(Styles::font(Styles::Regular, 12, QFont::DemiBold));
	setCount(count);
}

WarningRibbon::~WarningRibbon()
{
	delete ui;
}

void WarningRibbon::flip()
{
	ui->expanded = !ui->expanded;
	if(ui->expanded)
	{
		ui->leftImage->setStyleSheet(QStringLiteral("border: none; background-image: url(:/images/icon_ribbon_up.svg);"));
		ui->rightImage->setStyleSheet(QStringLiteral("border: none; background-image: url(:/images/icon_ribbon_up.svg);"));
	}
	else
	{
		ui->leftImage->setStyleSheet(QStringLiteral("border: none; background-image: url(:/images/icon_ribbon_down.svg);"));
		ui->rightImage->setStyleSheet(QStringLiteral("border: none; background-image: url(:/images/icon_ribbon_down.svg);"));
	}
	setCount(ui->count);
}

bool WarningRibbon::isExpanded() const
{
	return ui->expanded;
}

void WarningRibbon::setCount(int count)
{
	ui->count = count;
	if(ui->expanded)
		ui->details->setText(tr("Less"));
	else
		ui->details->setText(tr("%n message", "", count));
}

void WarningRibbon::changeEvent(QEvent* event)
{
	if (event->type() == QEvent::LanguageChange)
	{
		ui->retranslateUi(this);
		setCount(ui->count);
	}
	QWidget::changeEvent(event);
}
