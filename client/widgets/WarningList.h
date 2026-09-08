// SPDX-FileCopyrightText: Estonian Information System Authority
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "StyledWidget.h"

struct WarningText;

class WarningList: public StyledWidget
{
	Q_OBJECT
public:
	explicit WarningList(QWidget *parent = nullptr);

	void closeWarnings(int page);
	void showWarning(WarningText warningText);
	void updateWarnings(int page);
};
