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

#include "StyledWidget.h"

namespace Ui {
class WarningRibbon;
}

class WarningRibbon : public StyledWidget
{
	Q_OBJECT

public:
	WarningRibbon(const QList<QWidget*> *warnings, QWidget *parent = nullptr);
	~WarningRibbon();

	void change();
	bool isExpanded() const;
	void updateVisibility();

private:
	Ui::WarningRibbon *ui;
	bool expanded;
	const QList<QWidget*> *warnings;
};
