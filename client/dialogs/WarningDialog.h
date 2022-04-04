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
	};
	explicit WarningDialog(const QString &text, const QString &details, QWidget *parent = nullptr);
	explicit WarningDialog(const QString &text, QWidget *parent = nullptr);
	~WarningDialog() final;

	void addButton(ButtonText label, int ret, bool red = false);
	void addButton(const QString& label, int ret, bool red = false);
	void setButtonSize(int width, int margin);
	void setCancelText(ButtonText label);
	void setCancelText(const QString& label);
	void resetCancelStyle();
	static WarningDialog *show(const QString &text, const QString &details = {});
	static WarningDialog *show(QWidget *parent, const QString &text, const QString &details = {});

private:
	static QString buttonLabel(ButtonText label);

	Ui::WarningDialog *ui;
	WaitDialogHider hider;
};
