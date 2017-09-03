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

#include "LabelButton.h"

const QString LabelButton::styleTemplate("QLabel { background-color: %1; color: %2; text-decoration: none solid; }");
const QString LabelButton::linkTemplate("<a href=\"#%4\" style=\"background-color: %1; color: %2; text-decoration: none solid;\">%3</a>");

LabelButton::LabelButton(QWidget *parent)
: QLabel(parent)
{
}

void LabelButton::init( const QString &label, const QString &anchor, const QString &fgColor, const QString &bgColor )
{
    normalStyle = styleTemplate.arg(bgColor, fgColor);
    normalLink = linkTemplate.arg(bgColor, fgColor, label, anchor);
    hoverStyle = styleTemplate.arg(fgColor, bgColor);
    hoverLink = linkTemplate.arg(fgColor, bgColor, label, anchor);
}


void LabelButton::enterEvent(QEvent *ev)
{
    setStyleSheet(hoverStyle);
    setText(hoverLink);
}

void LabelButton::leaveEvent(QEvent *ev)
{
    setStyleSheet(normalStyle);
    setText(normalLink);
}
 