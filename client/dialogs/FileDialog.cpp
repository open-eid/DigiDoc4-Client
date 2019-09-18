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

#include "FileDialog.h"

#include "Application.h"
#include "dialogs/WaitDialog.h"

#include <QtCore/QCoreApplication>
#include <QtCore/QStandardPaths>
#include <QtCore/QTemporaryFile>
#include <QtCore/QSettings>
#include <QtWidgets/QMessageBox>
#ifdef Q_OS_WIN
#include <Shobjidl.h>
#include <Shlguid.h>
#endif

using namespace ria::qdigidoc4;

FileDialog::FileDialog( QWidget *parent )
:	QFileDialog( parent )
{
}

FileDialog::Options FileDialog::addOptions()
{
	if(qApp->arguments().contains(QStringLiteral("-noNativeFileDialog")))
		return DontUseNativeDialog;
	return nullptr;
}

QString FileDialog::create(const QFileInfo &fileInfo, const QString &extension, const QString &type)
{
	QString fileName = QDir::toNativeSeparators(fileInfo.dir().path() + QDir::separator() + fileInfo.completeBaseName() + extension);
#ifndef Q_OS_OSX
	// macOS App Sandbox restricts the rights of the application to write to the filesystem outside of
	// app sandbox; user must explicitly give permission to write data to the specific folders.
	if(QFile::exists(fileName))
	{
#endif
		WaitDialogHider hider;
		QString capitalized = type[0].toUpper() + type.mid(1);
		fileName = FileDialog::getSaveFileName(qApp->activeWindow(), Application::tr("Create %1").arg(type), fileName,
			QStringLiteral("%1 (*%2)").arg(capitalized, extension));
		if(!fileName.isEmpty())
			QFile::remove(fileName);
#ifndef Q_OS_OSX
	}
#endif

	return fileName;
}

QString FileDialog::createNewFileName(const QString &file, const QString &extension, const QString &type, const QString &defaultDir)
{
	const QFileInfo f(file);
	QString dir = defaultDir.isEmpty() ? f.absolutePath() : defaultDir;
	QString filePath = dir + QDir::separator() + f.fileName();
	return create(QFileInfo(filePath), extension, type);
}

FileType FileDialog::detect( const QString &filename )
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
		if(!file.open(QIODevice::ReadOnly))
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
}

ExtensionType FileDialog::extension(const QString &filename)
{
	static const QStringList exts {"bdoc", "ddoc", "asice", "sce", "asics", "scs", "edoc", "adoc"};
	const QFileInfo f( filename );
	if(!f.isFile())
		return ExtOther;
	if(exts.contains(f.suffix(), Qt::CaseInsensitive))
		return ExtSignature;
	if(!QString::compare(f.suffix(), QStringLiteral("cdoc"), Qt::CaseInsensitive))
		return ExtCrypto;
	if(!QString::compare(f.suffix(), QStringLiteral("pdf"), Qt::CaseInsensitive))
		return ExtPDF;
	return ExtOther;
}

bool FileDialog::fileIsWritable( const QString &filename )
{
	QFile f( filename );
	bool remove = !f.exists();
	bool result = f.open( QFile::WriteOnly|QFile::Append );
	if( remove )
		f.remove();
	return result;
}

QString FileDialog::fileSize( quint64 bytes )
{
	const quint64 kb = 1024;
	const quint64 mb = 1024 * kb;
	const quint64 gb = 1024 * mb;
	const quint64 tb = 1024 * gb;
	if( bytes >= tb )
		return QStringLiteral("%1 TB").arg(qreal(bytes) / tb, 0, 'f', 3);
	if( bytes >= gb )
		return QStringLiteral("%1 GB").arg(qreal(bytes) / gb, 0, 'f', 2);
	if( bytes >= mb )
		return QStringLiteral("%1 MB").arg(qreal(bytes) / mb, 0, 'f', 1);
	if( bytes >= kb )
		return QStringLiteral("%1 KB").arg(bytes / kb);
	return QStringLiteral("%1 B").arg(bytes);
}

QString FileDialog::getDir( const QString &dir )
{
#ifdef Q_OS_OSX
	Q_UNUSED(dir);
	QString path = QSettings().value(QStringLiteral("NSNavLastRootDirectory")).toString();
	path.replace('~', QDir::homePath());
	return path;
#else
	return !dir.isEmpty() ? dir : QSettings().value("lastPath",
		QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation)).toString();
#endif
}

QString FileDialog::getOpenFileName( QWidget *parent, const QString &caption,
	const QString &dir, const QString &filter, QString *selectedFilter, Options options )
{
	return result( QFileDialog::getOpenFileName( parent,
		caption, getDir( dir ), filter, selectedFilter, options|addOptions() ) );
}

QStringList FileDialog::getOpenFileNames( QWidget *parent, const QString &caption,
	const QString &dir, const QString &filter, QString *selectedFilter, Options options )
{
	return result( QFileDialog::getOpenFileNames( parent,
		caption, getDir( dir ), filter, selectedFilter,
#ifdef Q_OS_WIN
		DontResolveSymlinks|options|addOptions() ) );
#else
		options|addOptions() ) );
#endif
}

QString FileDialog::getExistingDirectory( QWidget *parent, const QString &caption,
	const QString &dir, Options options )
{
	QString res;
#ifdef Q_OS_WIN
	//https://bugreports.qt-project.org/browse/QTBUG-12655
	//https://bugreports.qt-project.org/browse/QTBUG-27359
	IFileOpenDialog *pfd = 0;
	CoCreateInstance( CLSID_FileOpenDialog, NULL, CLSCTX_INPROC, IID_PPV_ARGS(&pfd) );
	pfd->SetOptions( FOS_PICKFOLDERS );
	QString dest = QDir::toNativeSeparators( getDir( dir ) );
	IShellItem *folder = 0;
	if( !dest.isEmpty() && SUCCEEDED(SHCreateItemFromParsingName( LPCWSTR(dest.utf16()), 0, IID_PPV_ARGS(&folder) )) )
	{
		pfd->SetFolder(folder);
		folder->Release();
	}
	if( !caption.isEmpty() )
		pfd->SetTitle( LPCWSTR(caption.utf16()) );
	if( SUCCEEDED(pfd->Show( parent && parent->window() ? HWND(parent->window()->winId()) : 0 )) )
	{
		IShellItem *item = 0;
		if( SUCCEEDED(pfd->GetResult( &item )) )
		{
			LPWSTR path = 0;
			if( SUCCEEDED(item->GetDisplayName( SIGDN_FILESYSPATH, &path )) )
			{
				res = QString( (QChar*)path );
				CoTaskMemFree( path );
			}
			else
			{
				// Case it is Libraries/Favorites
				IEnumShellItems *items = 0;
				if( SUCCEEDED(item->BindToHandler( 0, BHID_EnumItems, IID_PPV_ARGS(&items) )) )
				{
					IShellItem *list = 0;
					if( items->Next( 1, &list, 0 ) == NOERROR )
					{
						LPWSTR path = 0;
						if( SUCCEEDED(list->GetDisplayName( SIGDN_FILESYSPATH, &path )) )
						{
							res = QFileInfo( QString( (QChar*)path ) + "/.." ).absoluteFilePath();
							CoTaskMemFree( path );
						}
						list->Release();
					}
					items->Release();
				}
			}
		}
		item->Release();
	}
	pfd->Release();
#else
	res = QFileDialog::getExistingDirectory( parent,
		caption, getDir( dir ), ShowDirsOnly|options|addOptions() );
#endif
	if(res.isEmpty())
		return res;
#ifdef Q_OS_WIN
	if( !QTemporaryFile( res + "/.XXXXXX" ).open() )
#else
	if( !QFileInfo( res ).isWritable() )
#endif
	{
		QMessageBox::warning( parent, caption,
			tr( "You don't have sufficient privileges to write this file into folder %1" ).arg( res ) );
		return QString();
	}

	return result( res );
}

QString FileDialog::getSaveFileName( QWidget *parent, const QString &caption,
	const QString &dir, const QString &filter, QString *selectedFilter, Options options )
{
	QString file;
	while( true )
	{
		file =  QFileDialog::getSaveFileName( parent,
			caption, dir, filter, selectedFilter, options|addOptions() );
		if( !file.isEmpty() && !fileIsWritable( file ) )
		{
			QMessageBox::warning( parent, caption,
				tr( "You don't have sufficient privileges to write this file into folder %1" ).arg( file ) );
		}
		else
			break;
	}
	return result( file );
}

QString FileDialog::result( const QString &str )
{
#ifndef Q_OS_OSX
	if(!str.isEmpty())
		QSettings().setValue("lastPath", QFileInfo(str).absolutePath());
#else
	QSettings().remove(QStringLiteral("lastPath"));
#endif
	return str;
}

QStringList FileDialog::result( const QStringList &list )
{
	QStringList l;
	for( const QString &str: list )
		l << result( str );
	return l;
}

QString FileDialog::tempPath(const QString &file)
{
	QDir tmp = QDir::temp();
	if(!tmp.exists(file))
		return tmp.path() + "/" + file;
	QFileInfo info(file);
	int i = 0;
	while(tmp.exists(QStringLiteral("%1_%2.%3").arg(info.baseName()).arg(i).arg(info.suffix())))
		++i;
	return QStringLiteral("%1/%2_%3.%4").arg(tmp.path()).arg(info.baseName()).arg(i).arg(info.suffix());
}

QString FileDialog::safeName(const QString &file)
{
	QString filename = file;
#if defined(Q_OS_WIN)
	filename.replace(QRegExp(QStringLiteral("[\\\\/*:?\"<>|]")), QStringLiteral("_"));
#elif defined(Q_OS_MAC)
	filename.replace(QRegExp(QStringLiteral("[\\\\/:]")), QStringLiteral("_"));
#else
	filename.replace(QRegExp(QStringLiteral("[\\\\/]")), QStringLiteral("_"));
#endif
	return filename;
}
