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

class FileItem : public Item
{
	Q_OBJECT

public:
	explicit FileItem( ria::qdigidoc4::ContainerState state, QWidget *parent = nullptr );
	explicit FileItem( const QString& file, ria::qdigidoc4::ContainerState state, QWidget *parent = nullptr );
	~FileItem();

	QString getFile();
	void stateChange(ria::qdigidoc4::ContainerState state) override;

signals:
	void open(FileItem* item);
	void download(FileItem* item);
	
protected:
	void enterEvent(QEvent *event) override;
	void leaveEvent (QEvent *event) override;
	void mouseReleaseEvent(QMouseEvent *event) override;

private:
	Ui::FileItem *ui;
};
