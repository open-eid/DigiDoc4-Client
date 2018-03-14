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

namespace Ui {
class WaitDialog;
}

class Overlay;

class WaitDialog : public QDialog
{
	Q_OBJECT

public:
	explicit WaitDialog(QWidget *parent = nullptr, Overlay *o = nullptr);
	~WaitDialog();

	int exec() override;
	void open() override;
	QString text();

	static WaitDialog* create(QWidget *parent, Overlay *o = nullptr);
	static void destroy();

	void closeOverlay();
	Overlay* detachOverlay();
	static WaitDialog* instance();
	void showOverlay();
	void setText(const QString &text);

private:
	Ui::WaitDialog *ui;
	Overlay *overlay = nullptr;
	static WaitDialog *waitDialog;
};
