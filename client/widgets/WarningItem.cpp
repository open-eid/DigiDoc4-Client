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

#include "WarningItem.h"
#include "ui_WarningItem.h"

#include "Styles.h"

WarningItem::WarningItem(const QString &msg, const QString &details, bool extLink, QWidget *parent)
: StyledWidget(parent)
, ui(new Ui::WarningItem)
, closable(true)
{
	ui->setupUi(this);
	ui->warningText->setFont(Styles::font(Styles::Regular, 14));
	ui->warningAction->setFont(Styles::font(Styles::Regular, 14, QFont::Bold));

	ui->warningText->setText(msg);
	ui->warningAction->setText(details);
	ui->warningAction->setOpenExternalLinks(extLink);
	connect(ui->warningAction, &QLabel::linkActivated, this, &WarningItem::linkActivated);
}

WarningItem::~WarningItem()
{
	delete ui;
}

bool WarningItem::isClosable() const 
{ 
	return closable;
}

void WarningItem::setClosable(bool closable)
{
	this->closable = closable;
}
