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

#include "DocumentModel.h"
#include "widgets/FileItem.h"
#include "widgets/ItemList.h"

#include <QWidget>

class QLabel;

class FileList : public ItemList
{
	Q_OBJECT

public:
	explicit FileList(QWidget *parent = nullptr);
	~FileList();

	void init(const QString &container, const QString &label = "Kontaineri failid");
	void addFile(const QString& file);
	void setModel(DocumentModel *documentModel);

signals:
	void addFiles(const QStringList &files);

protected:
	void remove(Item *item) override;

private slots:
	void open(FileItem *item) const;
	void save(FileItem *item);

private:
	void selectFile();
	void showDownload();
	
	QString container;
	DocumentModel *documentModel;
};
