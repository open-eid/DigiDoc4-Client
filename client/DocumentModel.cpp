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

#include "DocumentModel.h"

#include "Application.h"
#include "dialogs/FileDialog.h"
#include "dialogs/WarningDialog.h"

#include <QtCore/QJsonArray>
#include <QtCore/QFileInfo>
#include <QtCore/QProcessEnvironment>

bool DocumentModel::addFileCheck(const QString &container, QFileInfo file)
{
	// Check that container is not dropped into itself
	if(QFileInfo(container) == file)
	{
		WarningDialog::create()
			->withTitle(tr("Cannot add container to same container"))
			->withText(FileDialog::normalized(container))
			->setCancelText(WarningDialog::Cancel)
			->open();
		return false;
	}

	if(file.size() == 0)
	{
		WarningDialog::create()
			->withTitle(tr("Cannot add empty file to the container"))
			->withText(FileDialog::normalized(file.absoluteFilePath()))
			->open();
		return false;
	}
	QString fileName = file.fileName();
	for(int row = 0; row < rowCount(); row++)
	{
		if(fileName == data(row))
		{
			WarningDialog::create()
				->withTitle(tr("File is already in container"))
				->withText(FileDialog::normalized(fileName))
				->open();
			return false;
		}
	}

	return true;
}

void DocumentModel::addTempFiles(const QStringList &files)
{
	for(const QString &file: files)
	{
		addFile(file);
		addTempReference(file);
	}
}

QStringList DocumentModel::tempFiles() const
{
	QStringList copied;
	for(int i = 0, rows = rowCount(); i < rows; ++i)
	{
		if(QFileInfo f(save(i, FileDialog::tempPath(data(i)))); f.exists())
			copied.append(f.absoluteFilePath());
	}
	return copied;
}

bool DocumentModel::verifyFile(const QString &f)
{
	static const QJsonArray defaultArray {
			QStringLiteral("ddoc"), QStringLiteral("bdoc") ,QStringLiteral("edoc"), QStringLiteral("adoc"), QStringLiteral("asice"), QStringLiteral("cdoc"), QStringLiteral("asics"),
			QStringLiteral("txt"), QStringLiteral("doc"), QStringLiteral("docx"), QStringLiteral("odt"), QStringLiteral("ods"), QStringLiteral("tex"), QStringLiteral("wks"), QStringLiteral("wps"),
			QStringLiteral("wpd"), QStringLiteral("rtf"), QStringLiteral("xlr"), QStringLiteral("xls"), QStringLiteral("xlsx"), QStringLiteral("pdf"), QStringLiteral("key"), QStringLiteral("odp"),
			QStringLiteral("pps"), QStringLiteral("ppt"), QStringLiteral("pptx"), QStringLiteral("png"), QStringLiteral("jpg"), QStringLiteral("jpeg"), QStringLiteral("bmp"), QStringLiteral("ai"),
			QStringLiteral("gif"), QStringLiteral("ico"), QStringLiteral("ps"), QStringLiteral("psd"), QStringLiteral("tif"), QStringLiteral("tiff"), QStringLiteral("csv")};

	QJsonArray allowedExts = Application::confValue(QLatin1String("ALLOWED-EXTENSIONS")).toArray(defaultArray);
	if(allowedExts.contains(QJsonValue(QFileInfo(f).suffix().toLower())))
		return true;
	WarningDialog::create()
		->withTitle(tr("Failed to open file"))
		->withText(tr("A file with this extension cannot be opened in the DigiDoc4 Client. Download the file to view it."))
		->setCancelText(WarningDialog::OK)
		->open();
	return false;
}
