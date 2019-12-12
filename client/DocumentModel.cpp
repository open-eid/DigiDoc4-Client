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

#include <QtCore/QFileInfo>
#include <QtCore/QProcessEnvironment>

DocumentModel::DocumentModel(QObject *parent)
: QObject(parent)
{}
DocumentModel::~DocumentModel() = default;

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
#if defined(Q_OS_WIN)
	QStringList exts = QProcessEnvironment::systemEnvironment().value("PATHEXT").split(';');
	exts.append({".PIF", ".SCR", ".LNK"});
	WarningDialog dlg(tr("This is an executable file! "
		"Executable files may contain viruses or other malicious code that could harm your computer. "
		"Are you sure you want to launch this file?"), qApp->activeWindow());
	dlg.setCancelText(tr("NO"));
	dlg.addButton(tr("YES"), 1);
	if(exts.contains("." + QFileInfo(f).suffix(), Qt::CaseInsensitive) && dlg.exec() != 1)
		return false;
#else
	Q_UNUSED(f)
#endif
	return true;
}
