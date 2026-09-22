// SPDX-FileCopyrightText: Estonian Information System Authority
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include <QtWidgets/QMenuBar>

class MacMenuBar final: public QMenuBar
{
	Q_OBJECT
public:
	explicit MacMenuBar();

	QMenu* fileMenu() const;
	QMenu* helpMenu() const;
	QMenu* dockMenu() const;

private:
	bool eventFilter(QObject *o, QEvent *e) final;

	QMenu *file {};
	QMenu *help {};
	QMenu *dock {};
};
