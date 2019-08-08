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

#include <QtWidgets/QFileDialog>

#include "common_enums.h"

class FileDialog : public QFileDialog
{
	Q_OBJECT
public:
	explicit FileDialog(QWidget *parent = nullptr);

	static QString create(const QFileInfo &fileInfo, const QString &extension, const QString &type);
	static QString createNewFileName(const QString &file, const QString &extension, const QString &type, const QString &defaultDir);
	static ria::qdigidoc4::FileType detect(const QString &filename);
	static bool fileIsWritable( const QString &filename );
	static QString fileSize( quint64 bytes );
	static ria::qdigidoc4::ExtensionType extension(const QString &filename);
	static QString safeName(const QString &file);
	static QString tempPath(const QString &file);

	static QString getOpenFileName(QWidget *parent = nullptr, const QString &caption = QString(),
		const QString &dir = QString(), const QString &filter = QString(),
		QString *selectedFilter = nullptr, Options options = nullptr);
	static QStringList getOpenFileNames(QWidget *parent = nullptr, const QString &caption = QString(),
		const QString &dir = QString(), const QString &filter = QString(),
		QString *selectedFilter = nullptr, Options options = nullptr);
	static QString getExistingDirectory(QWidget *parent = nullptr, const QString &caption = QString(),
		const QString &dir = QString(), Options options = nullptr);
	static QString getSaveFileName(QWidget *parent = nullptr, const QString &caption = QString(),
		const QString &dir = QString(), const QString &filter = QString(),
		QString *selectedFilter = nullptr, Options options = nullptr);

private:
	static QString result( const QString &str );
	static QStringList result( const QStringList &list );
	static Options addOptions();
	static QString getDir( const QString &dir );
};
