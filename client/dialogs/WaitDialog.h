// SPDX-FileCopyrightText: Estonian Information System Authority
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include <QWidget>

class WaitDialogHider
{
public:
	WaitDialogHider();
	~WaitDialogHider();
};


class WaitDialogHolder
{
public:
	WaitDialogHolder(QWidget *parent, const QString &text, bool show = true);
	~WaitDialogHolder();
};
