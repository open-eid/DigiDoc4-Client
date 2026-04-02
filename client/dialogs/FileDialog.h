// SPDX-FileCopyrightText: Estonian Information System Authority
// SPDX-License-Identifier: LGPL-2.1-or-later

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
	static bool isSignedPDF(const QString &path);
	static void setFileZone(const QString &target, const QString &source);
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
		const QString &filename = {}, QString filter = {});

private:
	static QString result( const QString &str );
	static QStringList result( const QStringList &list );
	static QString getDir( const QString &dir );
};
