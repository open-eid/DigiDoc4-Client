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
: StyledWidget(parent)
, ui(new Ui::FileItem)
{
	ui->setupUi(this);
	ui->fileName->setFont(Styles::font(Styles::Regular, 14));
	ui->download->installEventFilter( new ButtonHoverFilter( ":/images/icon_download.svg", ":/images/icon_download_hover.svg", this ) );
	ui->remove->installEventFilter( new ButtonHoverFilter( ":/images/icon_remove.svg", ":/images/icon_remove_hover.svg", this ) );
	stateChange(state);
}

FileItem::FileItem( const QString& file, ContainerState state, QWidget *parent )
: FileItem( state, parent )
{
	const QFileInfo f( file );
	// TODO check if file
	// if( !f.isFile() ) return Other;

	ui->fileName->setText( f.fileName() );
	filePath = f.dir().path();
}

FileItem::~FileItem()
{
	delete ui;
}

void FileItem::stateChange(ContainerState state)
{
	switch(state)
	{
	case UnsignedContainer:
	case UnsignedSavedContainer:
		ui->download->hide();
		ui->remove->show();
		break;
	case SignedContainer:
		ui->download->show();
		ui->remove->hide();
		break;
	
	case UnencryptedContainer:
		ui->download->hide();
		ui->remove->show();
		break;
	case EncryptedContainer:
		ui->download->hide();
		ui->remove->hide();
		break;
	case DecryptedContainer:
		ui->download->show();
		ui->remove->hide();
		break;
	}
}
