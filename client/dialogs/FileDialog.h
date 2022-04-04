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

class FileDialog : public QFileDialog
{
	Q_OBJECT
public:
	enum FileType {
		SignatureDocument,
		CryptoDocument,
		Other
	};

	using QFileDialog::QFileDialog;

	static QString createNewFileName(const QString &file, bool signature, QWidget *parent);
	static FileType detect(const QString &filename);
	static bool fileIsWritable( const QString &filename );
	static int fileZone(const QString &path);
	static bool isSignedPDF(const QString &path);
	static void setFileZone(const QString &path, int zone);
	static void setReadOnly(const QString &path, bool readonly = true);
	static QString normalized(const QString &file);
	static QString safeName(const QString &file);
	static QString tempPath(const QString &file);

	static QString getOpenFileName(QWidget *parent = nullptr, const QString &caption = {},
		const QString &dir = {}, const QString &filter = {},
		QString *selectedFilter = nullptr, Options options = {});
	static QStringList getOpenFileNames(QWidget *parent = nullptr, const QString &caption = {},
		const QString &dir = {}, const QString &filter = {},
		QString *selectedFilter = nullptr, Options options = {});
	static QString getExistingDirectory(QWidget *parent = nullptr, const QString &caption = {},
		const QString &dir = {}, Options options = {});
	static QString getSaveFileName(QWidget *parent = nullptr, const QString &caption = {},
		const QString &dir = {}, const QString &filter = {},
		QString *selectedFilter = nullptr, Options options = {});

private:
	static QString result( const QString &str );
	static QStringList result( const QStringList &list );
	static QString getDir( const QString &dir );
};
