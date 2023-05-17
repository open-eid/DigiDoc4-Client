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

#include "Application.h"
#include "MainWindow.h"
#include "dialogs/SettingsDialog.h"

#include <QtCore/QEvent>

MacMenuBar::MacMenuBar()
	: file(addMenu(tr("&File")))
	, help(addMenu(tr("&Help")))
	, dock(new QMenu(this))
{
	qApp->installEventFilter(this);
	dock->setAsDockMenu();
	file->addAction(QString(), [] {
		if(auto *w = qobject_cast<MainWindow*>(Application::mainWindow()))
			w->showSettings(SettingsDialog::LicenseSettings);
	})->setMenuRole(QAction::AboutRole);
	file->addAction(QString(), [] {
		if(auto *w = qobject_cast<MainWindow*>(Application::mainWindow()))
			w->showSettings(SettingsDialog::GeneralSettings);
	})->setMenuRole(QAction::PreferencesRole);
}

QMenu* MacMenuBar::fileMenu() const { return file; }
QMenu* MacMenuBar::helpMenu() const { return help; }
QMenu* MacMenuBar::dockMenu() const { return dock; }

bool MacMenuBar::eventFilter(QObject *o, QEvent *e)
{
	if(e->type() == QEvent::LanguageChange)
	{
		file->setTitle(tr("&File"));
		help->setTitle(tr("&Help"));
	}
	return QMenuBar::eventFilter(o, e);
}
