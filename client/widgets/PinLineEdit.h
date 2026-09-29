// SPDX-FileCopyrightText: Estonian Information System Authority
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "widgets/LineEdit.h"

class QRegularExpressionValidator;

// A masked PIN input that enforces the accepted PIN length/charset.
// hasAcceptableInput() is true once a full, valid PIN has been entered, so
// callers can gate their confirm button on it (and connect to textChanged()).
class PinLineEdit final: public LineEdit
{
	Q_OBJECT
public:
	explicit PinLineEdit(QWidget *parent = nullptr);

	// Restrict input to [min, max] characters. numeric=true accepts only digits
	// (regular PIN); numeric=false accepts any character (e.g. Tempel certs).
	void setPinLen(int min, int max = 12, bool numeric = true);

private:
	QRegularExpressionValidator *m_validator;
};
