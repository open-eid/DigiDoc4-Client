/*
 * QDigiDocClient
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

class FileDialog : public QFileDialog
{
	Q_OBJECT
public:
	explicit FileDialog( QWidget *parent = 0 );

	static bool fileIsWritable( const QString &filename );
	static QString fileSize( quint64 bytes );
	static QString safeName(const QString &file);
	static QString tempPath(const QString &file);

	static QString getOpenFileName( QWidget *parent = 0, const QString &caption = QString(),
		const QString &dir = QString(), const QString &filter = QString(),
		QString *selectedFilter = 0, Options options = 0 );
	static QStringList getOpenFileNames( QWidget *parent = 0, const QString &caption = QString(),
		const QString &dir = QString(), const QString &filter = QString(),
		QString *selectedFilter = 0, Options options = 0 );
	static QString getExistingDirectory( QWidget *parent = 0, const QString &caption = QString(),
		const QString &dir = QString(), Options options = 0 );
	static QString getSaveFileName( QWidget *parent = 0, const QString &caption = QString(),
		const QString &dir = QString(), const QString &filter = QString(),
		QString *selectedFilter = 0, Options options = 0 );

private:
	static QString result( const QString &str );
	static QStringList result( const QStringList &list );
	static Options addOptions();
	static QString getDir( const QString &dir );
};
