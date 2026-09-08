// SPDX-FileCopyrightText: Estonian Information System Authority
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include <QObject>
#include <QtCore/QTemporaryDir>

#include <memory>

class QFileInfo;

class DocumentModel: public QObject
{
	Q_OBJECT
public:
	using QObject::QObject;

	virtual bool addFile(const QString &file, const QString &mime = QStringLiteral("application/octet-stream")) = 0;
	virtual void copyModel(DocumentModel *model);
	virtual QString data(int row) const = 0;
	virtual quint64 fileSize(int row) const = 0;
	virtual QString mime(int row) const = 0;
	virtual void open(int row);
	virtual bool removeRow(int row) = 0;
	virtual int rowCount() const = 0;
	virtual QString save(int row, const QString &path) const = 0;
	void clearTempFolder();
	QString saveTemp(int row) const;

signals:
	void added(const QString &file);

protected:
	bool addFileCheck(const QString &container, const QFileInfo &file) const;
	virtual QString containerName() const = 0;
	void removeTempFile(const QString &file);

private:
	mutable std::unique_ptr<QTemporaryDir> m_tempDir;
};
