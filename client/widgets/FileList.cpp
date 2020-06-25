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

#include "dialogs/FileDialog.h"
#include "dialogs/WarningDialog.h"
#include "effects/ButtonHoverFilter.h"
#include "widgets/FileItem.h"

#include <QDrag>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QMessageBox>
#include <QMimeData>
#include <QMouseEvent>

using namespace ria::qdigidoc4;


FileList::FileList(QWidget *parent)
: ItemList(parent)
{
	ui->download->setIcons(QStringLiteral("/images/icon_download.svg"), QStringLiteral("/images/icon_download_hover.svg"),
		QStringLiteral("/images/icon_download_pressed.svg"), 17, 17);
	ui->download->init(LabelButton::White, QString(), 0);
	ui->download->installEventFilter(
		new ButtonHoverFilter(QStringLiteral(":/images/icon_download.svg"), QStringLiteral(":/images/icon_download_hover.svg"), this));
	connect(ui->add, &LabelButton::clicked, this, &FileList::selectFile);
	connect(ui->download, &LabelButton::clicked, this, &FileList::saveAll);
}

void FileList::addFile( const QString& file )
{
	FileItem *item = new FileItem(file, state);
	item->installEventFilter(this);
	addWidget(item);

	connect(item, &FileItem::open, this, &FileList::open);
	connect(item, &FileItem::download, this, &FileList::save);

	updateDownload();
}

void FileList::changeEvent(QEvent* event)
{
	ItemList::changeEvent(event);
	if(!ui->count->isHidden())
		ui->count->setText(QString::number(items.size()));
}

void FileList::clear()
{
	ItemList::clear();
	documentModel = nullptr;
}

bool FileList::eventFilter(QObject *obj, QEvent *event)
{
	if(!qobject_cast<FileItem*>(obj))
		return ItemList::eventFilter(obj, event);
	switch(event->type())
	{
	case QEvent::MouseButtonPress:
	{
		QMouseEvent *mouse = static_cast<QMouseEvent*>(event);
		if (mouse->button() == Qt::LeftButton)
			obj->setProperty("dragStartPosition", mouse->pos());
		break;
	}
	case QEvent::MouseMove:
	{
		QMouseEvent *mouse = static_cast<QMouseEvent*>(event);
		if(!(mouse->buttons() & Qt::LeftButton))
			break;
		if((mouse->pos() - obj->property("dragStartPosition").toPoint()).manhattanLength()
			 < QApplication::startDragDistance())
			break;
		FileItem *fileItem = qobject_cast<FileItem*>(obj);
		if(!documentModel || !fileItem)
			break;
		int i = index(fileItem);
		if(i == -1)
			break;
		QString path = FileDialog::tempPath(fileItem->getFile());
		documentModel->save(i, path);
		documentModel->addTempReference(path);
		QMimeData *mimeData = new QMimeData;
		mimeData->setText(QFileInfo(path).fileName());
		mimeData->setUrls({ QUrl::fromLocalFile(path) });
		QDrag *drag = new QDrag(this);
		drag->setMimeData(mimeData);
		drag->exec(Qt::CopyAction);
		return true;
	}
	default: break;
	}
	return ItemList::eventFilter(obj, event);
}

void FileList::init(const QString &container, const QString &label)
{
	ItemList::init(ItemFile, label);
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
		emit removed(i);
}

void FileList::removeItem(int row)
{
	ItemList::removeItem(row);

	updateDownload();
}

void FileList::save(FileItem *item)
{
	int i;
	if(documentModel && (i = index(item)) != -1)
	{
		QString extension = QFileInfo(item->getFile()).suffix();
		QString capitalized = extension[0].toUpper() + extension.mid(1);
		QString dest = FileDialog::getSaveFileName(this,
			tr("Save file"), QFileInfo(container).dir().absolutePath() + QDir::separator() + item->getFile(),
			QStringLiteral("%1 (*%2)").arg(capitalized, extension));

		if(!dest.endsWith(QStringLiteral(".%1").arg(extension)) && !extension.isEmpty())
				dest.append(QStringLiteral(".%1").arg(extension));

		if(dest.isEmpty())
			return;
		QFile::remove( dest );
		documentModel->save(i, dest);
	}
}

void FileList::saveAll()
{
	QString dir = FileDialog::getExistingDirectory( this,
		tr("Select folder where files will be stored") );
	if( dir.isEmpty() )
		return;
	int b = QMessageBox::No;	// default
	for( int i = 0; i < documentModel->rowCount(); ++i )
	{
		QString dest = dir + QDir::separator() + documentModel->data(i);
		if( QFile::exists( dest ) )
		{
			if( b == QMessageBox::YesToAll )
			{
					QFile::remove( dest );
					documentModel->save( i, dest );
					continue;
			}
			WarningDialog dlg(tr("%1 already exists.<br />Do you want replace it?").arg( dest ), this);
			dlg.setButtonSize(60, 5);
			dlg.setCancelText(tr("NO"));
			dlg.addButton(tr("YES"), QMessageBox::Yes);
			dlg.addButton(tr("SAVE WITH OTHER NAME"), QMessageBox::No);
			dlg.addButton(tr("REPLACE ALL"), QMessageBox::YesToAll);
			dlg.addButton(tr("CANCEL"), QMessageBox::NoToAll);
			b = dlg.exec();

			if(b == QDialog::Rejected)
				continue;
			if(b == QMessageBox::NoToAll)
				break;
			if( b == QMessageBox::No )
			{
				dest = FileDialog::getSaveFileName( this, tr("Save file"), dest );
				if( dest.isEmpty() )
					continue;
			}
			else
				QFile::remove( dest );
		}
		documentModel->save( i, dest );
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
	if (!files.isEmpty())
		emit addFiles(files);
}

void FileList::setModel(DocumentModel *documentModel)
{
	this->documentModel = documentModel;
	disconnect(documentModel, &DocumentModel::added, nullptr, nullptr);
	disconnect(documentModel, &DocumentModel::removed, nullptr, nullptr);
	connect(documentModel, &DocumentModel::added, this, &FileList::addFile);
	connect(documentModel, &DocumentModel::removed, this, &FileList::removeItem);
	auto count = documentModel->rowCount();
	for(int i = 0; i < count; i++)
		addFile(documentModel->data(i));
}

void FileList::updateDownload()
{
	ui->download->setVisible(state & (UnsignedSavedContainer | SignedContainer | UnencryptedContainer) && !items.empty());
	ui->count->setVisible(state & (UnsignedSavedContainer | SignedContainer | UnencryptedContainer) && !items.empty());
	ui->count->setText(QString::number(items.size()));
}
