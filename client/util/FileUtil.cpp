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

#include "Application.h"
#include "FileDialog.h"
#include "dialogs/WaitDialog.h"

#include <QDir>
#include <QFileInfo>

using namespace ria::qdigidoc4;

FileType FileUtil::detect( const QString &filename )
{
	const QFileInfo f( filename );
	if( !f.isFile() ) return Other;

	QStringList exts = QStringList() << "bdoc" << "ddoc" << "asice" << "sce" << "asics" << "scs" << "edoc" << "adoc";
	if( exts.contains( f.suffix(), Qt::CaseInsensitive ) )
	{
		return SignatureDocument;
	}
	if( !QString::compare(f.suffix(), "cdoc", Qt::CaseInsensitive) )
	{
		return CryptoDocument;
	}
	if( !QString::compare(f.suffix(), "pdf", Qt::CaseInsensitive) )
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
	}

	return Other;
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
	QString dir = defaultDir.isEmpty() ?  f.absolutePath() : defaultDir;
	QString filePath = dir + QDir::separator() + f.completeBaseName();

	return create(QFileInfo(filePath), extension, type);
}
