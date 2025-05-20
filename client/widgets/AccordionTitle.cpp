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


#include "AccordionTitle.h"

AccordionTitle::AccordionTitle(QWidget *parent)
	: QCheckBox(parent)
{
	setStyleSheet(QStringLiteral(R"(
QCheckBox {
border-right: none; /*Workaround for right padding*/
color: #2F70B6;
font-family: Roboto, Helvetica;
font-size: 16px;
font-weight: 700;
}
QCheckBox::indicator {
width: 24px;
height: 24px;
}
QCheckBox::indicator:checked {
image: url(:/images/accordion_arrow_down.svg);
}
QCheckBox::indicator:unchecked {
image: url(:/images/accordion_arrow_right.svg);
}
)"));
}
