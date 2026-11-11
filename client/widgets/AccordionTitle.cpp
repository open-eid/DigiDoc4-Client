// SPDX-FileCopyrightText: Estonian Information System Authority
// SPDX-License-Identifier: LGPL-2.1-or-later

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
