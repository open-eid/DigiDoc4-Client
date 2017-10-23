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
#include "ui_ItemList.h"

#include "FileDialog.h"
#include "effects/ButtonHoverFilter.h"
#include "widgets/FileItem.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QMessageBox>

using namespace ria::qdigidoc4;


FileList::FileList(QWidget *parent)
: ItemList(parent)
{
	connect(ui->add, &LabelButton::clicked, this, &FileList::selectFile);
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

void FileList::init(const QString &container, const QString &label)
{
	ItemList::init(ItemFile, label, true);
	this->container = container;
}

void FileList::open(FileItem *item) const
{
	int i;
	if(documentModel && (i = index(item)) != -1)
		documentModel->open(i);
}

void FileList::remove(Item *item)
{
	int i;
	if(documentModel && (i = index(item)) != -1)
		documentModel->removeRows(i, 1);
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

void FileList::selectFile()
{
	if(!documentModel)
		return;

	const QFileInfo f(container);
	QString dir;
	if(f.isFile()) dir = f.dir().path();

	QStringList files = FileDialog::getOpenFileNames(this, tr("Add files"), dir);
	for(auto file: files)
		documentModel->addFile(file);
}

void FileList::showDownload()
{
	if(ui->download->isHidden())
	{
		ui->download->show();
		ui->download->installEventFilter( new ButtonHoverFilter( ":/images/icon_download.svg", ":/images/icon_download_hover.svg", this ) );
	}
}

void FileList::setModel(DocumentModel *documentModel)
{
	this->documentModel = documentModel;
	connect(documentModel, &DocumentModel::added, this, &FileList::addFile);
	connect(documentModel, &DocumentModel::removed, this, &FileList::removeItem);
	auto count = documentModel->rowCount();
	for(int i = 0; i < count; i++)
		addFile(documentModel->data(i));
}
