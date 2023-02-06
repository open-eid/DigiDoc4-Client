/*
 * QEstEidCommon
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

#include <QtWidgets/QMenuBar>

class MacMenuBar final: public QMenuBar
{
	Q_OBJECT
public:
	enum ActionType
	{
		AboutAction,
		CloseAction,
		PreferencesAction,
		HelpAction
	};

	explicit MacMenuBar();
	~MacMenuBar();

	QAction* addAction(ActionType type);
	QMenu* fileMenu() const;
	QMenu* helpMenu() const;
	QMenu* dockMenu() const;

private:
	bool eventFilter(QObject *o, QEvent *e) final;
	static QString typeName(ActionType type);

	QMenu		*file = nullptr;
	QMenu		*help = nullptr;
	QHash<ActionType,QAction*> actions;
	QMenu		*dock = new QMenu;
};
