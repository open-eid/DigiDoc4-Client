// SPDX-FileCopyrightText: Estonian Information System Authority
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once 

#include "common_enums.h"

#include <QWidget>

// Custom widget that overrides paint event in order to use stylesheets
// See https://wiki.qt.io/How_to_Change_the_Background_Color_of_QWidget
class StyledWidget : public QWidget
{
	Q_OBJECT

public:
	explicit StyledWidget(QWidget *parent = nullptr);
	~StyledWidget() override;

	virtual void stateChange(ria::qdigidoc4::ContainerState state);

protected:
	void paintEvent(QPaintEvent *ev) override;
};
