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

#include "widgets/Item.h"

namespace Ui {
class FileItem;
}

class QFont;
class QFontMetrics;

class FileItem : public Item
{
	Q_OBJECT

public:
	explicit FileItem(const QString& file, ria::qdigidoc4::ContainerState state, QWidget *parent = nullptr);
	~FileItem() final;

	QString getFile();
	QWidget* initTabOrder(QWidget *item) final;
	void stateChange(ria::qdigidoc4::ContainerState state) final;

signals:
	void open(FileItem* item);
	void download(FileItem* item);
	
private:
	bool event(QEvent *event) final;
	void setFileName();
	void setUnderline(bool enable);

	Ui::FileItem *ui;
	QString fileName;
};
