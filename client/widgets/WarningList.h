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

#include <QWidget>

namespace Ui {
class MainWindow;
}

class WarningItem;
struct WarningText;
class WarningRibbon;

class WarningList: public QObject
{
	Q_OBJECT
public:
	explicit WarningList(Ui::MainWindow *main, QWidget *parent = nullptr);

	void clearWarning(const QList<int> &warningType);
	void closeWarning(WarningItem *warning, bool force = false);
	void closeWarnings(int page);
	void showWarning(const WarningText &warningText);
	void updateWarnings();

signals:
	void warningClicked(const QString &link);

private:
	bool eventFilter(QObject *object, QEvent *event) final;
	void updateRibbon(int page, bool expanded);

	WarningRibbon *ribbon = nullptr;
	QList<WarningItem*> warnings;
	Ui::MainWindow *ui;
};
