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

using namespace ria::qdigidoc4;

FileItem::FileItem(QString file, ContainerState state, QWidget *parent)
	: Item(parent)
	, ui(new Ui::FileItem)
	, fileName(std::move(file))
{
	ui->setupUi(this);

	stateChange(state);

	connect(ui->fileName, &QPushButton::clicked, this, [this]{ emit open(this); setUnderline(false); });
	connect(ui->download, &QPushButton::clicked, this, [this]{ emit download(this); setUnderline(false); });
	connect(ui->remove, &QPushButton::clicked, this, [this]{ emit remove(this);});

	setFileName();
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
		setUnderline(true);
		break;
	case QEvent::Leave:
		setUnderline(false);
		break;
	case QEvent::Resize:
		setFileName();
		break;
	default: break;
	}
	return Item::event(event);
}

QString FileItem::getFile()
{
	return fileName;
}

void FileItem::initTabOrder(QWidget *item)
{
	setTabOrder(item, ui->fileName);
	setTabOrder(ui->fileName, ui->download);
	setTabOrder(ui->download, lastTabWidget());
}

QWidget* FileItem::lastTabWidget()
{
	return ui->remove;
}

void FileItem::setFileName()
{
	QString text = ui->fileName->fontMetrics().elidedText(fileName, Qt::ElideMiddle, ui->fileName->width());
	ui->fileName->setText(text.replace(QStringLiteral("&"), QStringLiteral("&&")));
}

void FileItem::setUnderline(bool enable)
{
	QFont font = ui->fileName->font();
	font.setUnderline(ui->fileName->isEnabled() && enable);
	ui->fileName->setFont(font);
}

void FileItem::stateChange(ContainerState state)
{
	ui->fileName->setDisabled(state == EncryptedContainer);
	ui->download->setVisible(state == UnsignedSavedContainer || state == SignedContainer || state == UnencryptedContainer);
	ui->remove->setHidden(state == SignedContainer || state == EncryptedContainer);
	setFileName();
}
