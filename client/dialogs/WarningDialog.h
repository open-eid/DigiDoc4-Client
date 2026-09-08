// SPDX-FileCopyrightText: Estonian Information System Authority
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include <QDialog>

#include "dialogs/WaitDialog.h"

namespace Ui {
class WarningDialog;
}

class WarningDialog final: public QDialog
{
	Q_OBJECT

public:
	enum ButtonText {
		Cancel,
		OK,
		NO,
		YES,
		Remove,
	};
	~WarningDialog() final;

	WarningDialog *addButton(ButtonText label, int ret, bool red = false);
	WarningDialog *addButton(const QString& label, int ret, bool red = false);
	WarningDialog *setCancelText(ButtonText label);
	WarningDialog *setCancelText(const QString& label);
	WarningDialog *resetCancelStyle(bool warning);
	WarningDialog *withTitle(const QString &text);
	WarningDialog *withText(const QString &text);
	WarningDialog *withDetails(const QString &details);

	static WarningDialog *create(QWidget *parent = {});

private:
	explicit WarningDialog(QWidget *parent = nullptr);

	static QString buttonLabel(ButtonText label);

	Ui::WarningDialog *ui;
	WaitDialogHider hider;
	QPushButton *cancel;
};
