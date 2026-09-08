// SPDX-FileCopyrightText: Estonian Information System Authority
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include <QtWidgets/QComboBox>

class ComboBox: public QComboBox
{
	Q_OBJECT
public:
	explicit ComboBox(QWidget *parent = nullptr);

	void hidePopup() final;
	void showPopup() final;
};
