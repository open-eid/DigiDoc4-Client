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

WarningRibbon::WarningRibbon(const QList<QWidget*> *warnings, QWidget *parent)
: StyledWidget(parent)
, ui(new Ui::WarningRibbon)
, expanded(false)
, warnings(warnings)
{
	ui->setupUi(this);
	ui->details->setFont(Styles::font(Styles::Regular, 12, QFont::DemiBold));
	ui->details->setText(tr("%n message", "", 1));

	updateVisibility();
}

WarningRibbon::~WarningRibbon()
{
	for(auto warning: *warnings)
		warning->show();

	delete ui;
}

void WarningRibbon::change()
{
	expanded = !expanded;
	if(expanded)
	{
		ui->details->setText(tr("Less"));
		ui->leftImage->setStyleSheet("border: none; background-image: url(:/images/icon_ribbon_up.svg);");
		ui->rightImage->setStyleSheet("border: none; background-image: url(:/images/icon_ribbon_up.svg);");
	}
	else
	{
		ui->details->setText(tr("%n message", "", warnings->size() - 1));
		ui->leftImage->setStyleSheet("border: none; background-image: url(:/images/icon_ribbon_down.svg);");
		ui->rightImage->setStyleSheet("border: none; background-image: url(:/images/icon_ribbon_down.svg);");
	}

	updateVisibility();
}

bool WarningRibbon::isExpanded() const
{
	return expanded;
}

void WarningRibbon::updateVisibility()
{
	if(expanded)
	{
		for(auto warning: *warnings)
			warning->show();
	}
	else
	{
		bool first = true;
		for(auto warning: *warnings)
		{
			warning->setVisible(first);
			first = false;
		}
	}
}