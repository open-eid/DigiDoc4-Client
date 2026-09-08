// SPDX-FileCopyrightText: Estonian Information System Authority
// SPDX-License-Identifier: LGPL-2.1-or-later

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
