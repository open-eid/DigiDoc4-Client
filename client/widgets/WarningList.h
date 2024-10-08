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

#include "StyledWidget.h"

struct WarningText;

class WarningList: public StyledWidget
{
	Q_OBJECT
public:
	explicit WarningList(QWidget *parent = nullptr);

	void clearMyEIDWarnings();
	void closeWarning(int warningType);
	void closeWarnings(int page);
	void showWarning(WarningText warningText);
	void updateWarnings(int page);

signals:
	void warningClicked(const QString &link);
};
