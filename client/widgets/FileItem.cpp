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

#include "FileItem.h"
#include "ui_FileItem.h"
#include "Styles.h"

#include <QDir>
#include <QFileInfo>

using namespace ria::qdigidoc4;

FileItem::FileItem(ContainerState state, QWidget *parent)
: Item(parent)
, ui(new Ui::FileItem)
, elided(false)
, fileFont(Styles::font(Styles::Regular, 14))
, fm(fileFont)
{
	ui->setupUi(this);
	ui->fileName->setFont(fileFont);
	ui->download->setIcons(QStringLiteral("/images/icon_download.svg"), QStringLiteral("/images/icon_download_hover.svg"), QStringLiteral("/images/icon_download_pressed.svg"), 1, 1, 17, 17);
	ui->download->init(LabelButton::White, QString(), 0);
	ui->remove->setIcons(QStringLiteral("/images/icon_remove.svg"), QStringLiteral("/images/icon_remove_hover.svg"), QStringLiteral("/images/icon_remove_pressed.svg"), 1, 1, 17, 17);
	ui->remove->init(LabelButton::White, QString(), 0);

	stateChange(state);

	connect(ui->download, &LabelButton::clicked, this, [this]{ emit download(this);});
	connect(ui->remove, &LabelButton::clicked, this, [this]{ emit remove(this);});
}

FileItem::FileItem( const QString& file, ContainerState state, QWidget *parent )
: FileItem( state, parent )
{
	const QFileInfo f(file);
	fileName = f.fileName();

	setMouseTracking(true);
	width = fm.width(fileName);
	setFileName(true);
}

FileItem::~FileItem()
{
	delete ui;
}

void FileItem::enterEvent(QEvent * /*event*/)
{
	if(isEnabled())
		ui->fileName->setStyleSheet(QStringLiteral("color: #363739; border: none; text-decoration: underline;"));
}

QString FileItem::getFile()
{
	return fileName;
}

void FileItem::leaveEvent(QEvent * /*event*/)
{
	ui->fileName->setStyleSheet(QStringLiteral("color: #363739; border: none;"));
}

void FileItem::mouseReleaseEvent(QMouseEvent * /*event*/)
{
	if(isEnabled())
		emit open(this);
}

void FileItem::resizeEvent(QResizeEvent * /*event*/)
{
	setFileName(false);
}

void FileItem::setFileName(bool force)
{
	if(ui->fileName->width() < width)
	{
		elided = true;
		ui->fileName->setText(fm.elidedText(fileName.toHtmlEscaped(), Qt::ElideMiddle, ui->fileName->width()));
	}
	else if(elided || force)
	{
		elided = false;
		ui->fileName->setText(fileName.toHtmlEscaped());
	}
}

void FileItem::stateChange(ContainerState state)
{
	setEnabled(state != EncryptedContainer);

	switch(state)
	{
	case UnsignedSavedContainer:
		ui->download->show();
		ui->remove->show();
		break;
	case SignedContainer:
		ui->download->show();
		ui->remove->hide();
		break;
	case EncryptedContainer:
		ui->download->hide();
		ui->remove->hide();
		break;
	default:
		ui->download->hide();
		ui->remove->show();
		break;
	}
}
