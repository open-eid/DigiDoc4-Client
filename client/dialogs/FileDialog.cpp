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
#include "dialogs/WarningDialog.h"

#include <QtCore/QCoreApplication>
#include <QtCore/QRegularExpression>
#include <QtCore/QStandardPaths>
#include <QtCore/QTemporaryFile>
#include <QtCore/QSettings>
#include <QtWidgets/QMessageBox>

#include <algorithm>

#ifdef Q_OS_WIN
#include <ShObjIdl.h>
#include <ShlGuid.h>

template <class T>
class CPtr
{
	T *d;
 public:
	CPtr(T *p = nullptr): d(p) {}
	~CPtr() { if(d) d->Release(); }
	inline T* operator->() const { return d; }
	inline operator T*() const { return d; }
	inline T** operator&() { return &d; }
};
#endif

#include <array>

QString FileDialog::createNewFileName(const QString &file, const QString &extension, const QString &type, const QString &defaultDir, QWidget *parent)
{
	const QFileInfo f(normalized(file));
	QString dir = defaultDir.isEmpty() ? f.absolutePath() : defaultDir;
	QString fileName = QDir::toNativeSeparators(dir + QDir::separator() + f.completeBaseName() + extension);
#ifndef Q_OS_OSX
	// macOS App Sandbox restricts the rights of the application to write to the filesystem outside of
	// app sandbox; user must explicitly give permission to write data to the specific folders.
	if(!QFile::exists(fileName))
		return fileName;
#endif
	QString capitalized = type[0].toUpper() + type.mid(1);
	fileName = FileDialog::getSaveFileName(parent, Application::tr("Create %1").arg(type), fileName,
		QStringLiteral("%1 (*%2)").arg(capitalized, extension));
	if(!fileName.isEmpty())
		QFile::remove(fileName);
	return fileName;
}

FileDialog::FileType FileDialog::detect(const QString &filename)
{
	const QFileInfo f(filename);
	if(!f.isFile())
		return Other;
	static const QStringList exts {"bdoc", "ddoc", "asice", "sce", "asics", "scs", "edoc", "adoc"};
	if(exts.contains(f.suffix(), Qt::CaseInsensitive))
		return SignatureDocument;
	if(!f.suffix().compare(QStringLiteral("cdoc"), Qt::CaseInsensitive))
		return CryptoDocument;
	if(isSignedPDF(filename))
		return SignatureDocument;
	return Other;
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

int FileDialog::fileZone(const QString &path)
{
#ifdef Q_OS_WIN
	CPtr<IZoneIdentifier> spzi;
	CPtr<IPersistFile> spf;
	DWORD dwZone = 0;
	if(SUCCEEDED(CoCreateInstance(CLSID_PersistentZoneIdentifier, nullptr, CLSCTX_INPROC, IID_PPV_ARGS(&spzi))) &&
		SUCCEEDED(spzi->QueryInterface(&spf)) &&
		SUCCEEDED(spf->Load(LPCWSTR(QDir::toNativeSeparators(path).utf16()), STGM_READ)) &&
		SUCCEEDED(spzi->GetId(&dwZone)))
		return int(dwZone);
#else
	Q_UNUSED(path)
#endif
	return -1;
}

bool FileDialog::isSignedPDF(const QString &path)
{
	const QFileInfo f(path);
	if(f.suffix().compare(QStringLiteral("pdf"), Qt::CaseInsensitive))
		return false;
	QFile file(path);
	if(!file.open(QIODevice::ReadOnly))
		return false;
	QByteArray blob = file.readAll();
	for(const QByteArray &token: {"adbe.pkcs7.detached", "adbe.pkcs7.sha1", "adbe.x509.rsa_sha1", "ETSI.CAdES.detached"})
	{
		if(blob.indexOf(token) > 0)
			return true;
	}
	return false;
}

void FileDialog::setFileZone(const QString &path, int zone)
{
	if(zone < 0)
		return;
#ifdef Q_OS_WIN
	CPtr<IZoneIdentifier> spzi;
	CPtr<IPersistFile> spf;
	if(SUCCEEDED(CoCreateInstance(CLSID_PersistentZoneIdentifier, nullptr, CLSCTX_INPROC, IID_PPV_ARGS(&spzi))) &&
		SUCCEEDED(spzi->SetId(zone)) &&
		SUCCEEDED(spzi->QueryInterface(&spf)))
		spf->Save(LPCWSTR(QDir::toNativeSeparators(path).utf16()), TRUE);
#else
	Q_UNUSED(path)
#endif
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
		caption, getDir(dir), filter, selectedFilter, options));
}

QStringList FileDialog::getOpenFileNames( QWidget *parent, const QString &caption,
	const QString &dir, const QString &filter, QString *selectedFilter, Options options )
{
	return result( QFileDialog::getOpenFileNames( parent,
		caption, getDir( dir ), filter, selectedFilter,
#ifdef Q_OS_WIN
		DontResolveSymlinks|options));
#else
		options));
#endif
}

QString FileDialog::getExistingDirectory( QWidget *parent, const QString &caption,
	const QString &dir, Options options )
{
	QString res;
#ifdef Q_OS_WIN
	Q_UNUSED(options)
	//https://bugreports.qt-project.org/browse/QTBUG-12655
	//https://bugreports.qt-project.org/browse/QTBUG-27359
	CPtr<IFileOpenDialog> pfd;
	CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC, IID_PPV_ARGS(&pfd));
	pfd->SetOptions( FOS_PICKFOLDERS );
	QString dest = QDir::toNativeSeparators( getDir( dir ) );
	CPtr<IShellItem> folder;
	if(!dest.isEmpty() && SUCCEEDED(SHCreateItemFromParsingName( LPCWSTR(dest.utf16()), nullptr, IID_PPV_ARGS(&folder))))
		pfd->SetFolder(folder);
	if( !caption.isEmpty() )
		pfd->SetTitle( LPCWSTR(caption.utf16()) );
	if(SUCCEEDED(pfd->Show(parent && parent->window() ? HWND(parent->window()->winId()) : nullptr)))
	{
		CPtr<IShellItem> item;
		if( SUCCEEDED(pfd->GetResult( &item )) )
		{
			LPWSTR path = nullptr;
			if( SUCCEEDED(item->GetDisplayName( SIGDN_FILESYSPATH, &path )) )
			{
				res = QString::fromWCharArray(path);
				CoTaskMemFree( path );
			}
			else
			{
				// Case it is Libraries/Favorites
				CPtr<IEnumShellItems> items;
				if(SUCCEEDED(item->BindToHandler(nullptr, BHID_EnumItems, IID_PPV_ARGS(&items))))
				{
					CPtr<IShellItem> list;
					if(items->Next(1, &list, nullptr) == NOERROR)
					{
						if( SUCCEEDED(list->GetDisplayName( SIGDN_FILESYSPATH, &path )) )
						{
							res = QFileInfo(QString::fromWCharArray(path) + "/..").absoluteFilePath();
							CoTaskMemFree( path );
						}
					}
				}
			}
		}
	}
#else
	QFileDialog dlg(parent, caption, getDir(dir));
	dlg.setFileMode(QFileDialog::Directory);
	dlg.setOptions(ShowDirsOnly|options);
	dlg.setLabelText(QFileDialog::Accept, tr("Choose"));
	if(!dlg.exec())
		return {};
	res = dlg.selectedFiles().first();
#endif
	if(res.isEmpty())
		return res;
#ifdef Q_OS_WIN
	// https://bugreports.qt.io/browse/QTBUG-74291
	if(!QTemporaryFile(res.replace("\\", "/") + "/.XXXXXX").open())
#else
	if( !QFileInfo( res ).isWritable() )
#endif
	{
		WarningDialog(tr( "You don't have sufficient privileges to write this file into folder %1" ).arg( res ), parent).exec();
		return {};
	}

	return result( res );
}

QString FileDialog::getSaveFileName( QWidget *parent, const QString &caption,
	const QString &dir, const QString &filter, QString *selectedFilter, Options options )
{
	QString file;
	while( true )
	{
		file = QFileDialog::getSaveFileName(parent,
			caption, normalized(dir), filter, selectedFilter, options);
		if( !file.isEmpty() && !fileIsWritable( file ) )
		{
			WarningDialog(tr( "You don't have sufficient privileges to write this file into folder %1" ).arg( file ), parent).exec();
		}
		else
			break;
	}
	return result( file );
}

QString FileDialog::normalized(const QString &data)
{
	static constexpr std::array<const unsigned char[3],5> list = {{
		{0xE2, 0x80, 0x8E}, // \u200E LEFT-TO-RIGHT MARK
		{0xE2, 0x80, 0x8F}, // \u200F RIGHT-TO-LEFT MARK
		{0xE2, 0x80, 0xAA}, // \u202A LEFT-TO-RIGHT EMBEDDING
		{0xE2, 0x80, 0xAB}, // \u202B RIGHT-TO-LEFT EMBEDDING
		{0xE2, 0x80, 0xAE}, // \u202E RIGHT-TO-LEFT OVERRIDE
	}};
	QString result = data.normalized(QString::NormalizationForm_C);
	for (const unsigned char *replace: list)
		result = result.remove((const char*)replace);
	return result;
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
	QFileInfo info(file);
	QString filename = info.fileName();
#if defined(Q_OS_WIN)
	static const QStringList disabled { "CON", "PRN", "AUX", "NUL",
										"COM1", "COM2", "COM3", "COM4", "COM5", "COM6", "COM7", "COM8", "COM9",
										"LPT1", "LPT2", "LPT3", "LPT4", "LPT5", "LPT6", "LPT7", "LPT8", "LPT9" };
	if(disabled.contains(info.baseName(), Qt::CaseInsensitive))
		filename = QStringLiteral("___.") + info.suffix();
	filename.replace(QRegularExpression(QStringLiteral("[\\\\/*:?\"<>|]")), QStringLiteral("_"));
#elif defined(Q_OS_MAC)
	filename.replace(QRegularExpression(QStringLiteral("[\\\\/:]")), QStringLiteral("_"));
#else
	filename.replace(QRegularExpression(QStringLiteral("[\\\\/]")), QStringLiteral("_"));
#endif
	return filename;
}
