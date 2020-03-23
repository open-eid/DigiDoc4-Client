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
	connect(dock, &QMenu::triggered, this, &MacMenuBar::activateWindow);
	dock->setAsDockMenu();
}

MacMenuBar::~MacMenuBar()
{
	//delete dock;
}

void MacMenuBar::activateWindow(QAction *a)
{
	if(QWidget *w = a->data().value<QWidget*>())
	{
		w->activateWindow();
		w->showNormal();
		w->raise();
	}
}

QAction* MacMenuBar::addAction(ActionType type, const QObject *receiver, const char *member)
{
	QAction *a = file->addAction(typeName(type), receiver, member);
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
	if(QWidget *w = qobject_cast<QWidget*>(o))
	{
		switch(e->type())
		{
		case QEvent::Close:
		case QEvent::Destroy:
			for(QAction *a : windowGroup->actions())
				if(w == a->data().value<QWidget*>())
					delete a;
			if(windowGroup->actions().isEmpty() && dockSeparator)
			{
				delete dockSeparator;
				dockSeparator = nullptr;
			}
			break;
		case QEvent::WindowActivate:
		{
			QWidget *main = w->topLevelWidget();
			for(QAction *a : windowGroup->actions())
				a->setChecked(main == a->data().value<QWidget*>());
			if(!windowGroup->checkedAction())
			{
				QAction *a = new QAction(title(w), dock);
				a->setCheckable(true);
				a->setData(QVariant::fromValue(main));
				a->setActionGroup(windowGroup);

				if(!dockSeparator)
					dockSeparator = dock->insertSeparator(dock->actions().value(0));
				dock->insertAction(dockSeparator, a);
			}
			break;
		}
		case QEvent::WindowTitleChange:
			for(QAction *a : windowGroup->actions())
				if(w == a->data().value<QWidget*>())
					a->setText(title(w));
			break;
		default: break;
		}
	}
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

QString MacMenuBar::title(QObject *o) const
{
	QString title = o->property("windowTitle").toString();
	title = title.isEmpty() ? o->property("windowFilePath").toString() : title;
	title.remove("[*]");
	return title;
}

QString MacMenuBar::typeName(ActionType type) const
{
	switch(type)
	{
	case AboutAction: return tr("Info");
	case CloseAction: return tr("Close");
	case PreferencesAction: return tr("Settings");
	default: return {};
	}
}
