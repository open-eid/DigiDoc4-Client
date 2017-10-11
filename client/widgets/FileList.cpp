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

#include "FileList.h"

#include "FileDialog.h"
#include "widgets/FileItem.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QMessageBox>

using namespace ria::qdigidoc4;

FileList::FileList(QWidget *parent)
: ItemList(parent)
{
}

FileList::~FileList()
{
}

void FileList::addFile( const QString& file )
{
	FileItem *item = new FileItem(file, state);
	addWidget(item);

	connect(item, &FileItem::open, this, &FileList::open);
	connect(item, &FileItem::download, this, &FileList::save);

	if(state & (SignedContainer | DecryptedContainer) && items.size() > 1)
		showDownload();
}

int FileList::index(StyledWidget *item) const
{
	auto it = std::find(items.begin(), items.end(), item);
	if(it != items.end())
		return std::distance(items.begin(), it);

	return -1;
}

void FileList::init(const QString &container, const QString &label)
{
	ItemList::init(ItemList::File, label, true);
	this->container = container;
}

void FileList::open(FileItem *item) const
{
	int i;
	if(documentModel && (i = index(item)) != -1)
		documentModel->open(i);
}

void FileList::save(FileItem *item)
{
	int i;
	if(documentModel && (i = index(item)) != -1)
	{
		QDir dir = QFileInfo(container).dir();
		QString dest = dir.absolutePath() + QDir::separator() + item->getFile();
		if(QFile::exists(dest))
		{
			dest = FileDialog::getSaveFileName(this, tr("Save file"), dest);
			if(!dest.isEmpty())
				QFile::remove( dest );
		}
		if(!dest.isEmpty())
			documentModel->save(i, dest);
	}
		
}

void FileList::setModel(DocumentModel *documentModel)
{
	this->documentModel = documentModel;
	auto count = documentModel->rowCount();
	for(int i = 0; i < count; i++)
		addFile(documentModel->data(i));
}
