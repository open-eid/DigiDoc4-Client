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

#include <QWidget>
#include <QPainter>
#include <QSvgWidget>

#include <memory>


namespace Ui {
class AccordionTitle;
}

class Accordion;

class AccordionTitle : public StyledWidget
{
	Q_OBJECT

public:
	explicit AccordionTitle(QWidget *parent = nullptr);
	~AccordionTitle();

	void init(Accordion* accordion, bool open, const QString& caption, QWidget* content);
	void openSection();
	void closeSection();

protected:
	void mouseReleaseEvent(QMouseEvent *event) override;

private:
	Ui::AccordionTitle *ui;
	std::unique_ptr<QSvgWidget> icon;
	QWidget* content;
	Accordion* accordion;
};
