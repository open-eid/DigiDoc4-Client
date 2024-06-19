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
