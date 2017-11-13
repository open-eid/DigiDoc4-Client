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
class WarningItem;
}


struct WarningText {
	QString text;
	QString details;
	bool external;
	QString property;
	int page;

	WarningText(const QString &text, const QString &details = QString(), int page = -1, const QString &property = QString());
	WarningText(const QString &text, const QString &details, bool external, const QString &property);
};


class WarningItem : public StyledWidget
{
	Q_OBJECT

public:
	WarningItem(const WarningText &warningText, QWidget *parent = nullptr);
	~WarningItem();
	
	bool appearsOnPage(int page) const;
	int page() const;

signals:
	void linkActivated(const QString& link);

protected:
	void changeEvent(QEvent* event) override;

private:
	Ui::WarningItem *ui;
	int context;
    WarningText warnText;
};
