// SPDX-FileCopyrightText: Estonian Information System Authority
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include <QWidget>

class Overlay final : public QWidget
{
	Q_OBJECT
public:
	Overlay(QWidget *runner);
	Overlay(QWidget *runner, QWidget *parent);
	~Overlay() final;

protected:
	void paintEvent(QPaintEvent *event) final;
};
