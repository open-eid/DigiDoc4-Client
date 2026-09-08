// SPDX-FileCopyrightText: Estonian Information System Authority
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "common_enums.h"

#include <QPushButton>

class MainAction final : public QPushButton
{
	Q_OBJECT

public:
	explicit MainAction(QWidget *parent);

	void setEncrypt(bool encrypt);

private:
	void changeEvent(QEvent* event) override;
	bool eventFilter(QObject *watched, QEvent *event) override;
	void update();

	bool isEncrypt = false;
};
