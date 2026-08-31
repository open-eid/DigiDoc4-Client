// SPDX-FileCopyrightText: Estonian Information System Authority
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "widgets/LineEdit.h"

class QRegularExpressionValidator;

class PinLineEdit final: public LineEdit
{
	Q_OBJECT
public:
	explicit PinLineEdit(QWidget *parent = nullptr);

	void setPinLen(int min, int max = 12, bool numeric = true);

private:
	QRegularExpressionValidator *m_validator;
};
