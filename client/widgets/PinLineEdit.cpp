// SPDX-FileCopyrightText: Estonian Information System Authority
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "widgets/PinLineEdit.h"

#include <QtGui/QRegularExpressionValidator>

PinLineEdit::PinLineEdit(QWidget *parent)
	: LineEdit(parent)
	, m_validator(new QRegularExpressionValidator(this))
{
	setEchoMode(QLineEdit::Password);
	setValidator(m_validator);
	setPinLen(4);
}

void PinLineEdit::setPinLen(int min, int max, bool numeric)
{
	setMaxLength(max);
	m_validator->setRegularExpression(QRegularExpression(QStringLiteral("^%1{%2,%3}$")
		.arg(numeric ? QStringLiteral("\\d") : QStringLiteral("."))
		.arg(min).arg(max)));
}
