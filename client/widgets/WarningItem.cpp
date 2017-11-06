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


WarningText::WarningText(const QString &text, const QString &details, int page)
: text(text)
, details(details)
, external(false)
, page(page)
{}

WarningText::WarningText(const QString &text, const QString &details, bool external, const QString &property)
: text(text)
, details(details)
, external(external)
, property(property)
, page(-1)
{}


WarningItem::WarningItem(const WarningText &warningText, QWidget *parent)
: StyledWidget(parent)
, ui(new Ui::WarningItem)
, context(warningText.page)
{
	ui->setupUi(this);
	ui->warningText->setFont(Styles::font(Styles::Regular, 14));
	ui->warningAction->setFont(Styles::font(Styles::Regular, 14, QFont::Bold));

	ui->warningText->setText(warningText.text);
	ui->warningAction->setText(warningText.details);
	ui->warningAction->setOpenExternalLinks(warningText.external);

	if(!warningText.property.isEmpty())
		setProperty(warningText.property.toLatin1(), true);

	connect(ui->warningAction, &QLabel::linkActivated, this, &WarningItem::linkActivated);
}

WarningItem::~WarningItem()
{
	delete ui;
}

bool WarningItem::appearsOnPage(int page) const
{
	return page == context || context == -1;
}

int WarningItem::page() const
{
	return context;
}