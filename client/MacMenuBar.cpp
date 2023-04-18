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

#include "MacMenuBar.h"

#include <QtCore/QEvent>
#include <QtWidgets/QApplication>

MacMenuBar::MacMenuBar()
	: file(addMenu(tr("&File")))
	, help(addMenu(tr("&Help")))
{
	qApp->installEventFilter(this);
	dock->setAsDockMenu();
}

MacMenuBar::~MacMenuBar()
{
	//delete dock;
}

QAction* MacMenuBar::addAction(ActionType type)
{
	QAction *a = file->addAction(typeName(type));
	switch(type)
	{
	case AboutAction: a->setMenuRole(QAction::AboutRole); break;
	case CloseAction: a->setShortcut(Qt::CTRL + Qt::Key_W); break;
	case PreferencesAction: a->setMenuRole(QAction::PreferencesRole); break;
	default: break;
	}
	actions[type] = a;
	return a;
}

QMenu* MacMenuBar::dockMenu() const { return dock; }

bool MacMenuBar::eventFilter(QObject *o, QEvent *e)
{
	switch(e->type())
	{
	case QEvent::LanguageChange:
		file->setTitle(tr("&File"));
		help->setTitle(tr("&Help"));
		for(auto i = actions.constBegin(); i != actions.constEnd(); ++i)
			i.value()->setText(typeName(i.key()));
		break;
	default: break;
	}
	return QMenuBar::eventFilter(o, e);
}

QMenu* MacMenuBar::fileMenu() const { return file; }
QMenu* MacMenuBar::helpMenu() const { return help; }

QString MacMenuBar::typeName(ActionType type)
{
	switch(type)
	{
	case AboutAction: return tr("Info");
	case CloseAction: return tr("Close");
	case PreferencesAction: return tr("Settings");
	default: return {};
	}
}
