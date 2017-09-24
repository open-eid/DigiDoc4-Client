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

#include "common_enums.h"

#include <QWidget>

// Custom widget that overrides paint event in order to use stylesheets
// See https://wiki.qt.io/How_to_Change_the_Background_Color_of_QWidget
class StyledWidget : public QWidget
{
	Q_OBJECT

public:
	explicit StyledWidget(QWidget *parent = nullptr);
	~StyledWidget();

	virtual void stateChange(ria::qdigidoc4::ContainerState state) {};

protected:
	void paintEvent(QPaintEvent *ev) override;
};
