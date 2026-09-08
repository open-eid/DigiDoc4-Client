// SPDX-FileCopyrightText: Estonian Information System Authority
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "DocumentModel.h"
#include "widgets/FileItem.h"
#include "widgets/ItemList.h"

#include <QWidget>

class QLabel;

class FileList final : public ItemList
{
	Q_OBJECT

public:
	explicit FileList(QWidget *parent = nullptr);

	void addFile(const QString& file);
	void clear() final;
	void init(const QString &container,
		const char *label = QT_TRANSLATE_NOOP("ItemList", "Container files"));
	void removeItem(int row) final;
	void setModel(DocumentModel *documentModel);
	void stateChange(ria::qdigidoc4::ContainerState state) final;

signals:
	void addFiles(const QStringList &files);

private:
	void open(FileItem *item) const;
	void save(FileItem *item);
	void saveAll();
	bool eventFilter(QObject *obj, QEvent *event) final;
	void selectFile();
	void updateDownload();
	
	QString container;
	DocumentModel *documentModel = nullptr;
};
