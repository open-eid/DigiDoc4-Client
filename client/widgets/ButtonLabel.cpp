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

 #include "ButtonLabel.h"
 
ButtonLabel::ButtonLabel(QWidget *parent)
: QLabel(parent)
{
}

void ButtonLabel::enterEvent(QEvent *ev)
{
    label = text();
    style = styleSheet();
    setStyleSheet("QLabel { background-color: #006eb5; color: #ffffff; text-decoration: none solid rgb(255, 255, 255); }");
    setText("<a href=\"#add-files\" style=\"background-color: #006eb5; color: #ffffff; text-decoration: none solid rgb(255, 255, 255);\">+ Lisa veel faile</a>");
}

void ButtonLabel::leaveEvent(QEvent *ev)
{
    setStyleSheet(style);
    setText(label);
}
 