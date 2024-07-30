/*
 * QEstEidCommon
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

#include "Diagnostics.h"

#include "Common.h"

#include <QtCore/QProcess>
#include <QtCore/QRegularExpression>
#include <QtCore/QSettings>
#include <QtCore/QTextStream>
#include <QtCore/QVector>
#include <QtNetwork/QSslSocket>

#include <qt_windows.h>

using namespace Qt::StringLiterals;

static QString getUserRights()
{
	HANDLE hToken {};
	if ( !OpenThreadToken( GetCurrentThread(), TOKEN_QUERY, TRUE, &hToken ) )
	{
		if ( GetLastError() != ERROR_NO_TOKEN )
			return Diagnostics::tr( "Unknown - error %1" ).arg( GetLastError() );
		if ( !OpenProcessToken( GetCurrentProcess(), TOKEN_QUERY, &hToken ) )
			return Diagnostics::tr( "Unknown - error %1" ).arg( GetLastError() );
	}

	DWORD dwLength {};
	if(!GetTokenInformation(hToken, TokenGroups, nullptr, dwLength, &dwLength))
	{
		if( GetLastError() != ERROR_INSUFFICIENT_BUFFER )
			return Diagnostics::tr( "Unknown - error %1" ).arg( GetLastError() );
	}

	QByteArray tokenGroupsBuffer(dwLength, 0);
	PTOKEN_GROUPS pGroup = PTOKEN_GROUPS(tokenGroupsBuffer.data());
	if ( !GetTokenInformation( hToken, TokenGroups, pGroup, dwLength, &dwLength ) )
		return Diagnostics::tr("Unknown - error %1").arg(GetLastError());

	QString rights = Diagnostics::tr( "User" );
	SID_IDENTIFIER_AUTHORITY NtAuthority = SECURITY_NT_AUTHORITY;
	PSID AdministratorsGroup {};
	if( AllocateAndInitializeSid( &NtAuthority, 2, SECURITY_BUILTIN_DOMAIN_RID,
			DOMAIN_ALIAS_RID_ADMINS, 0, 0, 0, 0, 0, 0, &AdministratorsGroup ) )
	{
		for ( DWORD dwIndex = 0; dwIndex < pGroup->GroupCount; dwIndex++ )
		{
			if ( EqualSid( AdministratorsGroup, pGroup->Groups[dwIndex].Sid ) )
			{
				rights = Diagnostics::tr( "Administrator" );
				break;
			}
		}
		FreeSid(AdministratorsGroup);
	}

	return rights;
}

QStringList Diagnostics::packages(const QStringList &names, bool withName)
{
	QStringList packages;
	for(const QString &group: {u"HKEY_LOCAL_MACHINE"_s, u"HKEY_CURRENT_USER"_s})
	{
		static const QVector<QSettings::Format> formats = []() -> QVector<QSettings::Format> {
			if(QSysInfo::currentCpuArchitecture().contains("64"_L1))
				return {QSettings::Registry32Format, QSettings::Registry64Format};
			return {QSettings::Registry32Format};
		}();
		for(QSettings::Format format: formats)
		{
			QSettings s(u"%1\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Uninstall"_s.arg(group), format);
			for(const QString &key: s.childGroups())
			{
				s.beginGroup(key);
				QString name = s.value("/DisplayName"_L1).toString();
				QString version = s.value("/DisplayVersion"_L1).toString();
				QString type = s.value("/ReleaseType"_L1).toString();
				if(!type.contains("Update"_L1, Qt::CaseInsensitive) &&
					!name.contains("Update"_L1, Qt::CaseInsensitive) &&
					name.contains(QRegularExpression(names.join('|').prepend('^'), QRegularExpression::CaseInsensitiveOption)))
					packages.append(packageName(name, version, withName));
				s.endGroup();
			}
		}
	}
	return packages;
}

void Diagnostics::run()
{
	QString info;
	QTextStream s( &info );

	s << "<b>" << tr("Locale:") << "</b> ";
	QLocale::Language language = QLocale::system().language();
	QString locale = (language == QLocale::C ? "English/United States" : QLocale::languageToString( language ) );
	CPINFOEX CPInfoEx {};
	if( GetCPInfoEx( GetConsoleCP(), 0, &CPInfoEx ) != 0 )
		locale.append(" / ").append(QString::fromWCharArray(CPInfoEx.CodePageName));
	s << locale << "<br />";
	emit update( info );
	info.clear();

	s << "<b>" << tr("User rights: ") << "</b>" << getUserRights() << "<br />";
	emit update( info );
	info.clear();

	QStringList base = packages({"eID software"}, false);
	if( !base.isEmpty() )
		s << "<b>" << tr("Base version:") << "</b> " << base.join( "<br />" ) << "<br />";
	s << "<b>" << tr("Application version:") << "</b> " << QCoreApplication::applicationVersion() << " (" << QSysInfo::WordSize << " bit)<br />";
	emit update( info );
	info.clear();

	s << "<b>" << tr("OS:") << "</b> " << Common::applicationOs() << "<br /><br /";
	emit update( info );
	info.clear();

	s << "<b>" << tr("Libraries") << ":</b><br />"
		<< "QT (" << qVersion() << ")<br />"
		<< "OpenSSL build (" << QSslSocket::sslLibraryBuildVersionString() << ")<br />"
		<< "OpenSSL current (" << QSslSocket::sslLibraryVersionString() << ")<br />";

	QByteArray path = qgetenv("PATH");
	qputenv("PATH", path
		+ ";C:\\Program Files\\Open-EID"
		+ ";C:\\Program Files\\EstIDMinidriver Minidriver"
		+ ";C:\\Program Files (x86)\\Open-EID"
		+ ";C:\\Program Files (x86)\\EstIDMinidriver Minidriver");
	SetDllDirectory(LPCWSTR(qApp->applicationDirPath().utf16()));
	static const QStringList dlls{
		"digidocpp", "qdigidoc4.exe", "EsteidShellExtension", "id-updater.exe",
		"EstIDMinidriver", "EstIDMinidriver64", "web-eid.exe",
		"zlib1", "libxml2", "libxmlsec1", "libxmlsec1-openssl"};
	for(const QString &lib: dlls)
	{
		DWORD infoHandle {};
		DWORD sz = GetFileVersionInfoSize(LPCWSTR(lib.utf16()), &infoHandle);
		if( !sz )
			continue;
		QByteArray data(int(sz * 2), 0);
		if( !GetFileVersionInfoW( LPCWSTR(lib.utf16()), 0, sz, data.data() ) )
			continue;
		VS_FIXEDFILEINFO *info {};
		UINT len {};
		if( !VerQueryValueW( data.constData(), L"\\", (LPVOID*)&info, &len ) )
			continue;
		s << u"%1 (%2.%3.%4.%5)"_s.arg(lib)
			.arg( HIWORD(info->dwFileVersionMS) )
			.arg( LOWORD(info->dwFileVersionMS) )
			.arg( HIWORD(info->dwFileVersionLS) )
			.arg( LOWORD(info->dwFileVersionLS) ) << "<br />";
	}
	qputenv("PATH", path);
	SetDllDirectory(nullptr);

	s << "<br />";
	emit update( info );
	info.clear();

	QString atrfiltr = tr("Not found");
	QString certprop = tr("Not found");
	if(SC_HANDLE h = OpenSCManager(nullptr, nullptr, SC_MANAGER_CONNECT))
	{
		if( SC_HANDLE s = OpenService( h, L"atrfiltr", SERVICE_QUERY_STATUS ) )
		{
			SERVICE_STATUS status {};
			QueryServiceStatus( s, &status );
			atrfiltr = status.dwCurrentState == SERVICE_RUNNING ? tr("Running") : tr("Not running");
			CloseServiceHandle( s );
		}
		if( SC_HANDLE s = OpenService( h, L"CertPropSvc", SERVICE_QUERY_STATUS ))
		{
			SERVICE_STATUS status {};
			QueryServiceStatus( s, &status );
			certprop = status.dwCurrentState == SERVICE_RUNNING ? tr("Running") : tr("Not running");
			CloseServiceHandle( s );
		}
		CloseServiceHandle( h );
	}
	s << "<br /><b>" << tr("ATRfiltr service status: ") << "</b " << atrfiltr
		<< "<br /><b>" << tr("Certificate Propagation service status: ") << "</b> " << certprop << "<br />";

	generalInfo( s );
	emit update( info );
	info.clear();

	s << "<br /><br /><b>" << tr("Browsers:") << "</b><br />"
		<< packages({"Mozilla Firefox", "Google Chrome", "Microsoft EDGE"}).join("<br />"_L1) << "<br /><br />";
	emit update( info );
	info.clear();
}
