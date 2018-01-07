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

#include "FileUtil.h"
#ifdef Q_OS_OSX
#include "MacUtil.h"
#endif

#include "Application.h"
#include "FileDialog.h"
#include "dialogs/WaitDialog.h"
#include "dialogs/WarningDialog.h"

#include <QDir>
#include <QFileInfo>

using namespace ria::qdigidoc4;

FileType FileUtil::detect( const QString &filename )
{
	ExtensionType ext = extension(filename);

	switch(ext)
	{
		case ExtSignature:
			return SignatureDocument;
		case ExtCrypto:
			return CryptoDocument;
		case ExtPDF:
		{
			QFile file(filename);

			if( !file.open( QIODevice::ReadOnly ) )
				return Other;

			QByteArray blob = file.readAll();

			for(auto token: {"adbe.pkcs7.detached", "adbe.pkcs7.sha1", "adbe.x509.rsa_sha1", "ETSI.CAdES.detached"})
			{
				if(blob.indexOf(QByteArray(token)) > 0)
					return SignatureDocument;
			}
			return Other;
		}
		default:
			return Other;
	}

	return Other;
}

ExtensionType FileUtil::extension(const QString &filename)
{
	static QStringList exts = QStringList() << "bdoc" << "ddoc" << "asice" << "sce" << "asics" << "scs" << "edoc" << "adoc";
	const QFileInfo f( filename );
	if( !f.isFile() ) return ExtOther;

	if(exts.contains(f.suffix(), Qt::CaseInsensitive))
		return ExtSignature;
	if(!QString::compare(f.suffix(), "cdoc", Qt::CaseInsensitive))
		return ExtCrypto;
	if(!QString::compare(f.suffix(), "pdf", Qt::CaseInsensitive))
		return ExtPDF;

	return ExtOther;
}

QString FileUtil::create(const QFileInfo &fileInfo, const QString &extension, const QString &type)
{
	QString fileName = QDir::toNativeSeparators(fileInfo.dir().path() + QDir::separator() + fileInfo.completeBaseName() + extension);
	if(QFile::exists(fileName))
	{
		auto hider = WaitDialog::hider();
		QString capitalized = type[0].toUpper() + type.mid(1);
		fileName = FileDialog::getSaveFileName(qApp->activeWindow(), qApp->tr("Create %1").arg(type), fileName,
						QString("%1 (*%2)").arg(capitalized).arg(extension));
		if(!fileName.isEmpty())
			QFile::remove( fileName );
	}

	return fileName;
}

QString FileUtil::createFile(const QString &file, const QString &extension, const QString &type)
{
	const QFileInfo f( file );
	if( !f.isFile() ) return QString();

	return create(f, extension, type);
}

QString FileUtil::createNewFileName(const QString &file, const QString &extension, const QString &type, const QString &defaultDir)
{
	const QFileInfo f( file );
	QString dir = defaultDir.isEmpty() ? f.absolutePath() : defaultDir;
	QString filePath = dir + QDir::separator() + f.completeBaseName();

	QString containerPath = create(QFileInfo(filePath), extension, type);
#ifdef Q_OS_OSX
	// macOS App Sandbox restricts the rights of the application to write to the filesystem outside of
	// app sandbox; user must explicitly give permission to write data to the specific folders.
	if(!MacUtil::isWritable(dir.toUtf8().constData()))
	{
		auto hider = WaitDialog::hider();
		WarningDialog dlg(qApp->tr("ALLOW_ACCESS").arg(dir), qApp->activeWindow());
		dlg.setCancelText(qApp->tr("CANCEL"));
		dlg.addButton(qApp->tr("ALLOW"), 1);
		dlg.exec();
		if(dlg.result() != 1)
			return QString();

		QString containerDir = FileDialog::getExistingDirectory(qApp->activeWindow(), "Select destination folder", dir);
		if(containerDir.isEmpty())
			return QString();

		MacUtil::bookmark(containerDir.toUtf8().constData());
		QString newFilePath = containerDir + QDir::separator() + f.completeBaseName();
		if(dir != containerDir)
			containerPath = create(QFileInfo(newFilePath), extension, type);
	}
#endif
	return containerPath;
}
