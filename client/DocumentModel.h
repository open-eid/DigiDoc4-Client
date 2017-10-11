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
#include <QString>

class DocumentModel: public QObject
{
	Q_OBJECT
public:
	DocumentModel(QObject *parent = nullptr);
	virtual ~DocumentModel();

	virtual void addFile(const QString &file, const QString &mime = "application/octet-stream") = 0;
	virtual QString data(int row) const = 0;
	virtual bool removeRows(int row, int count) = 0;
	virtual int rowCount() const = 0;
	virtual QString save(int row, const QString &path) const = 0;

signals:
	void removed(int row);
	void added(const QString &file);

public slots:
	virtual void open(int row) = 0;
};
