// SPDX-FileCopyrightText: Estonian Information System Authority
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "Label.h"

#include <QStyle>

Label::Label(QWidget *parent)
	: QLabel(parent)
{}

QString Label::label() const
{
	return _label;
}

void Label::setLabel(QString label)
{
	if (label == _label)
		return;
	_label = std::move(label);
	parentWidget()->style()->unpolish(this);
	parentWidget()->style()->polish(this);
}
