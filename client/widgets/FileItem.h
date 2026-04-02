// SPDX-FileCopyrightText: Estonian Information System Authority
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "widgets/Item.h"

namespace Ui {
class FileItem;
}

class FileItem final : public Item
{
	Q_OBJECT

public:
	explicit FileItem(QString file, ria::qdigidoc4::ContainerState state, QWidget *parent = nullptr);
	~FileItem() final;

	QString getFile();
	void initTabOrder(QWidget *item) final;
	QWidget* lastTabWidget() final;
	void stateChange(ria::qdigidoc4::ContainerState state) final;

signals:
	void open(FileItem* item);
	void download(FileItem* item);

private:
	bool event(QEvent *event) final;
	void setFileName();
	void setUnderline(bool enable);

	Ui::FileItem *ui;
	QString fileName;
};
