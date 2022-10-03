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

#include <common/Configuration.h>

#include <QtCore/QJsonObject>
#include <QtCore/QJsonArray>
#include <QtCore/QFileInfo>
#include <QtCore/QProcessEnvironment>

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
	int rows = rowCount();
	for(int i = 0; i < rows; ++i)
	{
		QFileInfo f(save(i, FileDialog::tempPath(data(i))));
		if(f.exists())
			copied << f.absoluteFilePath();
	}

	return copied;
}

bool DocumentModel::verifyFile(const QString &f)
{
	QJsonObject obj;
#ifdef CONFIG_URL
	obj = qApp->conf()->object();
#endif

	static const QJsonArray defaultArray {
			QStringLiteral("ddoc"), QStringLiteral("bdoc") ,QStringLiteral("edoc"), QStringLiteral("adoc"), QStringLiteral("asice"), QStringLiteral("cdoc"), QStringLiteral("asics"),
			QStringLiteral("txt"), QStringLiteral("doc"), QStringLiteral("docx"), QStringLiteral("odt"), QStringLiteral("ods"), QStringLiteral("tex"), QStringLiteral("wks"), QStringLiteral("wps"),
			QStringLiteral("wpd"), QStringLiteral("rtf"), QStringLiteral("xlr"), QStringLiteral("xls"), QStringLiteral("xlsx"), QStringLiteral("pdf"), QStringLiteral("key"), QStringLiteral("odp"),
			QStringLiteral("pps"), QStringLiteral("ppt"), QStringLiteral("pptx"), QStringLiteral("png"), QStringLiteral("jpg"), QStringLiteral("jpeg"), QStringLiteral("bmp"), QStringLiteral("ai"),
			QStringLiteral("gif"), QStringLiteral("ico"), QStringLiteral("ps"), QStringLiteral("psd"), QStringLiteral("tif"), QStringLiteral("tiff"), QStringLiteral("csv")};

	QJsonArray allowedExts = obj.value(QLatin1String("ALLOWED-EXTENSIONS")).toArray(defaultArray);

	if(!allowedExts.contains(QJsonValue(QFileInfo(f).suffix().toLower()))){
		WarningDialog dlg(tr("A file with this extension cannot be opened in the DigiDoc4 Client. Download the file to view it."), qApp->activeWindow());
		dlg.setCancelText(tr("OK"));
		dlg.exec();
		return false;
	}

	return true;
}
