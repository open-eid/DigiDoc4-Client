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

namespace Ui { class WarningItem; }


struct WarningText {

	QString text, details;
	int counter = 0, page = -1;
	ria::qdigidoc4::WarningType warningType = ria::qdigidoc4::NoWarning;

	WarningText(QString text, QString details = {}, int page = -1);
	WarningText(ria::qdigidoc4::WarningType warningType, int counter = 0);
};


class WarningItem : public StyledWidget
{
	Q_OBJECT

public:
	WarningItem(WarningText warningText, QWidget *parent = nullptr);
	~WarningItem() final;
	
	int page() const;
	ria::qdigidoc4::WarningType warningType() const;

signals:
	void linkActivated(const QString& link);

protected:
	void changeEvent(QEvent* event) override;

private:
	void lookupWarning();

	Ui::WarningItem *ui;
	WarningText warnText;
};
