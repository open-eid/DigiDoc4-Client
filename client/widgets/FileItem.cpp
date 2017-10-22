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
#include "effects/ButtonHoverFilter.h"

#include <QDir>
#include <QFileInfo>

using namespace ria::qdigidoc4;

FileItem::FileItem(ContainerState state, QWidget *parent)
: Item(parent)
, ui(new Ui::FileItem)
{
	ui->setupUi(this);
	ui->fileName->setFont(Styles::font(Styles::Regular, 14));
	ui->download->installEventFilter( new ButtonHoverFilter( ":/images/icon_download.svg", ":/images/icon_download_hover.svg", this ) );
	ui->remove->installEventFilter( new ButtonHoverFilter( ":/images/icon_remove.svg", ":/images/icon_remove_hover.svg", this ) );

	stateChange(state);

	connect(ui->download, &QToolButton::clicked, [this](){ emit download(this);});
	connect(ui->remove, &QToolButton::clicked, [this](){ emit remove(this);});
}

FileItem::FileItem( const QString& file, ContainerState state, QWidget *parent )
: FileItem( state, parent )
{
	const QFileInfo f( file );
	setMouseTracking(true);
	ui->fileName->setText( f.fileName() );
}

FileItem::~FileItem()
{
	delete ui;
}

void FileItem::enterEvent(QEvent *event)
{
	Q_UNUSED (event);
	if(isEnabled())
		ui->fileName->setStyleSheet("color: #363739; border: none; text-decoration: underline;");
}

QString FileItem::getFile()
{
	return ui->fileName->text();
}

void FileItem::leaveEvent(QEvent *event)
{
	Q_UNUSED (event);
	ui->fileName->setStyleSheet("color: #363739; border: none;");
}

void FileItem::mouseReleaseEvent(QMouseEvent *event)
{
	if(isEnabled())
		emit open(this);
}


void FileItem::stateChange(ContainerState state)
{
	setEnabled(state != EncryptedContainer);

	switch(state)
	{
	case SignedContainer:
		ui->download->show();
		ui->remove->hide();
		break;
	case EncryptedContainer:
		ui->download->hide();
		ui->remove->hide();
		break;
	case DecryptedContainer:
		ui->download->show();
		ui->remove->hide();
		break;
	default:
		ui->download->hide();
		ui->remove->show();
		break;
	}
}
