// SPDX-FileCopyrightText: Estonian Information System Authority
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include <QObject>

class QFileInfo;

class DocumentModel: public QObject
{
	Q_OBJECT
public:
	using QObject::QObject;

	virtual bool addFile(const QString &file, const QString &mime = QStringLiteral("application/octet-stream")) = 0;
	virtual void addTempFiles(const QStringList &files);
	virtual void addTempReference(const QString &file) = 0;
	virtual QString data(int row) const = 0;
	virtual quint64 fileSize(int row) const = 0;
	virtual QString mime(int row) const = 0;
	virtual void open(int row) = 0;
	virtual bool removeRow(int row) = 0;
	virtual int rowCount() const = 0;
	virtual QString save(int row, const QString &path) const = 0;
	virtual QStringList tempFiles() const;

signals:
	void added(const QString &file);

protected:
	bool addFileCheck(const QString &container, QFileInfo file);
	static bool verifyFile(const QString &f);
};
