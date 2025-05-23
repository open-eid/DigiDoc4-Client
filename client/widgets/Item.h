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

class QLabel;
class SslCertificate;

class Item : public StyledWidget
{
	Q_OBJECT

public:
	using StyledWidget::StyledWidget;

	virtual void idChanged(const SslCertificate &cert);
	virtual void initTabOrder(QWidget *item);
	virtual QWidget* lastTabWidget();

signals:
	void add(Item* item);
	void remove(Item* item);
};


class LabelItem : public Item
{
	Q_OBJECT

public:
	LabelItem(const char *text, QWidget *parent = nullptr);
	void initTabOrder(QWidget *item) override;

private:
	void changeEvent(QEvent *event) override;
	QLabel *label;
	const char *_text;
};
