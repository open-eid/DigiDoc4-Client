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

#include <QtCore/QCoreApplication>
#include <QtCore/QCryptographicHash>
#include <QtCore/QDir>
#include <QtCore/QDirIterator>
#include <QtCore/QFileInfo>
#include <QtCore/QJsonArray>
#include <QtCore/QUrl>
#include <QtGui/QDesktopServices>

void DocumentModel::clearTempFolder()
{
	if(m_tempDir)
	{
		QDirIterator it(m_tempDir->path(), QDir::Files, QDirIterator::Subdirectories);
		while(it.hasNext())
			FileDialog::setReadOnly(it.next(), false);
	}
	m_tempDir.reset();
}

QString DocumentModel::saveTemp(int row) const
{
	if(!m_tempDir)
		m_tempDir = std::make_unique<QTemporaryDir>(
			QDir::tempPath() + QDir::separator() + QCoreApplication::applicationName() + QStringLiteral("_XXXXXX"));
	if(!m_tempDir->isValid())
		return {};
	QString name = data(row);
	QDir dir(m_tempDir->path());
	QString folder = QFileInfo(name).path();
	if(folder != QLatin1String("."))
	{
		QString hash = QCryptographicHash::hash(folder.toUtf8(), QCryptographicHash::Sha256).left(8).toHex();
		dir.mkpath(hash);
		dir = QDir(dir.filePath(hash));
	}
	return save(row, dir.filePath(FileDialog::safeName(name)));
}

void DocumentModel::removeTempFile(const QString &file)
{
	if(!m_tempDir)
		return;
	QDir dir(m_tempDir->path());
	QString folder = QFileInfo(file).path();
	if(folder != QLatin1String("."))
	{
		QString hash = QCryptographicHash::hash(folder.toUtf8(), QCryptographicHash::Sha256).left(8).toHex();
		dir = QDir(dir.filePath(hash));
	}
	QString path = dir.filePath(FileDialog::safeName(file));
	FileDialog::setReadOnly(path, false);
	QFile::remove(path);
}

bool DocumentModel::addFileCheck(const QString &container, const QFileInfo &file) const
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

void DocumentModel::open(int row)
{
	if(row >= rowCount())
		return;
	static const QJsonArray defaultArray {
			QStringLiteral("ddoc"), QStringLiteral("bdoc"), QStringLiteral("edoc"), QStringLiteral("adoc"), QStringLiteral("asice"), QStringLiteral("cdoc"), QStringLiteral("asics"),
			QStringLiteral("txt"), QStringLiteral("doc"), QStringLiteral("docx"), QStringLiteral("odt"), QStringLiteral("ods"), QStringLiteral("tex"), QStringLiteral("wks"), QStringLiteral("wps"),
			QStringLiteral("wpd"), QStringLiteral("rtf"), QStringLiteral("xlr"), QStringLiteral("xls"), QStringLiteral("xlsx"), QStringLiteral("pdf"), QStringLiteral("key"), QStringLiteral("odp"),
			QStringLiteral("pps"), QStringLiteral("ppt"), QStringLiteral("pptx"), QStringLiteral("png"), QStringLiteral("jpg"), QStringLiteral("jpeg"), QStringLiteral("bmp"), QStringLiteral("ai"),
			QStringLiteral("gif"), QStringLiteral("ico"), QStringLiteral("ps"), QStringLiteral("psd"), QStringLiteral("tif"), QStringLiteral("tiff"), QStringLiteral("csv")};
	QJsonArray allowedExts = Application::confValue(QLatin1String("ALLOWED-EXTENSIONS")).toArray(defaultArray);
	if(!allowedExts.contains(QJsonValue(QFileInfo(data(row)).suffix().toLower())))
	{
		WarningDialog::create()
			->withTitle(tr("Failed to open file"))
			->withText(tr("A file with this extension cannot be opened in the DigiDoc4 Client. Download the file to view it."))
			->setCancelText(WarningDialog::OK)
			->open();
		return;
	}
	QString path = saveTemp(row);
	if(path.isEmpty())
		return;
	FileDialog::setReadOnly(path);
	if(!containerName().endsWith(QLatin1String(".pdf"), Qt::CaseInsensitive) && FileDialog::isSignedPDF(path))
		Application::showClient({ std::move(path) }, false, false, true);
	else
		QDesktopServices::openUrl(QUrl::fromLocalFile(path));
}

void DocumentModel::copyModel(DocumentModel *model)
{
	if(!model)
		return;
	for(int i = 0, rows = model->rowCount(); i < rows; ++i)
	{
		QString path = model->saveTemp(i);
		if(path.isEmpty())
			continue;
		addFile(path);
	}
	if(!m_tempDir)
		m_tempDir = std::exchange(model->m_tempDir, nullptr);
}
