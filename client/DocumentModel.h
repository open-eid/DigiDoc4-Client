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
#pragma once

#include <QObject>

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
	void removed(int row);

protected:
	static bool verifyFile(const QString &f);
};
