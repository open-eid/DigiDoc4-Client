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

#include "widgets/StyledWidget.h"

namespace Ui {
class AccordionTitle;
}

class AccordionTitle final : public StyledWidget
{
	Q_OBJECT

public:
	explicit AccordionTitle(QWidget *parent = nullptr);
	~AccordionTitle() final;

	void init(bool open, const QString &caption, QWidget *content);
	void init(bool open, const QString &caption, const QString &accessible, QWidget *content);
	bool isOpen() const;
	void openSection(AccordionTitle *opened);
	void setText(const QString &caption, const QString &accessible);
	void setSectionOpen(bool open = true);
	void setVisible(bool visible) final;

Q_SIGNALS:
	void closed(AccordionTitle* opened);
	void opened(AccordionTitle* opened);

private:
	bool event(QEvent *e) final;

	Ui::AccordionTitle *ui;
	bool _isOpen = true;
	QWidget* content = nullptr;
};
