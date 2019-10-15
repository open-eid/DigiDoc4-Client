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

#include <QtCore/QDir>
#include <QtCore/QFileInfo>
#include <QtGui/QKeyEvent>
#include <QtWidgets/QAccessibleWidget>

using namespace ria::qdigidoc4;

FileItem::FileItem(ContainerState state, QWidget *parent)
	: Item(parent)
	, ui(new Ui::FileItem)
{
	ui->setupUi(this);
	ui->fileName->setFont(Styles::font(Styles::Regular, 14));
	ui->download->setIcons(QStringLiteral("/images/icon_download.svg"), QStringLiteral("/images/icon_download_hover.svg"), QStringLiteral("/images/icon_download_pressed.svg"), 17, 17);
	ui->download->init(LabelButton::White, QString(), 0);
	ui->remove->setIcons(QStringLiteral("/images/icon_remove.svg"), QStringLiteral("/images/icon_remove_hover.svg"), QStringLiteral("/images/icon_remove_pressed.svg"), 17, 17);
	ui->remove->init(LabelButton::White, QString(), 0);

	stateChange(state);

	connect(ui->download, &LabelButton::clicked, this, [this]{ emit download(this);});
	connect(ui->remove, &LabelButton::clicked, this, [this]{ emit remove(this);});
}

FileItem::FileItem( const QString& file, ContainerState state, QWidget *parent )
	: FileItem(state, parent)
{
	fileName = QFileInfo(file).fileName();
	setMouseTracking(true);
	width = ui->fileName->fontMetrics().width(fileName);
	setFileName(true);
}

FileItem::~FileItem()
{
	delete ui;
}

bool FileItem::event(QEvent *event)
{
	switch(event->type())
	{
	case QEvent::Enter:
		if(isEnabled)
			ui->fileName->setStyleSheet(QStringLiteral("color: #363739; border: none; text-decoration: underline;"));
		break;
	case QEvent::Leave:
		ui->fileName->setStyleSheet(QStringLiteral("color: #363739; border: none;"));
		break;
	case QEvent::MouseButtonRelease:
		if(isEnabled)
			emit open(this);
		break;
	case QEvent::Resize:
		setFileName(false);
		break;
	case QEvent::KeyRelease:
		if(QKeyEvent *ke = static_cast<QKeyEvent*>(event))
		{
			if(isEnabled && (ke->key() == Qt::Key_Enter || ke->key() == Qt::Key_Space))
				emit open(this);
		}
		break;
	default: break;
	}
	return Item::event(event);
}

QString FileItem::getFile()
{
	return fileName;
}

QWidget* FileItem::initTabOrder(QWidget *item)
{
	setTabOrder(item, ui->fileName);
	setTabOrder(ui->fileName, ui->download);
	setTabOrder(ui->download, ui->remove);
	return ui->remove;
}

void FileItem::setFileName(bool force)
{
	if(ui->fileName->width() < width)
	{
		elided = true;
		ui->fileName->setText(ui->fileName->fontMetrics().elidedText(fileName.toHtmlEscaped(), Qt::ElideMiddle, ui->fileName->width()));
	}
	else if(elided || force)
	{
		elided = false;
		ui->fileName->setText(fileName.toHtmlEscaped());
	}
}

void FileItem::stateChange(ContainerState state)
{
	isEnabled = state != EncryptedContainer;
	ui->fileName->setAccessibleDescription(isEnabled ? tr("To open file press space or enter") : QString());
	ui->download->setVisible(state == UnsignedSavedContainer || state == SignedContainer);
	ui->remove->setHidden(state == SignedContainer || state == EncryptedContainer);
}
