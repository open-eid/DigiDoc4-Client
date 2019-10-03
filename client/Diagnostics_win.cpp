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
#include <QtCore/QSettings>
#include <QtCore/QTextStream>

#include <qt_windows.h>

static QString getUserRights()
{
	HANDLE hToken = nullptr;
	if ( !OpenThreadToken( GetCurrentThread(), TOKEN_QUERY, TRUE, &hToken ) )
	{
		if ( GetLastError() != ERROR_NO_TOKEN )
			return Diagnostics::tr( "Unknown - error %1" ).arg( GetLastError() );
		if ( !OpenProcessToken( GetCurrentProcess(), TOKEN_QUERY, &hToken ) )
			return Diagnostics::tr( "Unknown - error %1" ).arg( GetLastError() );
	}

	DWORD dwLength = 0;
	if(!GetTokenInformation(hToken, TokenGroups, nullptr, dwLength, &dwLength))
	{
		if( GetLastError() != ERROR_INSUFFICIENT_BUFFER )
			return Diagnostics::tr( "Unknown - error %1" ).arg( GetLastError() );
	}

	PTOKEN_GROUPS pGroup = PTOKEN_GROUPS(GlobalAlloc(GPTR, dwLength));
	if ( !GetTokenInformation( hToken, TokenGroups, pGroup, dwLength, &dwLength ) )
	{
		if ( pGroup )
			GlobalFree( pGroup );
		return Diagnostics::tr( "Unknown - error %1" ).arg( GetLastError() );;
	}

	QString rights = Diagnostics::tr( "User" );
	SID_IDENTIFIER_AUTHORITY NtAuthority = SECURITY_NT_AUTHORITY;
	PSID AdministratorsGroup = nullptr;
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
	}

	if ( AdministratorsGroup )
		FreeSid( AdministratorsGroup );
	if ( pGroup )
		GlobalFree( pGroup );

	return rights;
}

void Diagnostics::run()
{
	QString info;
	QTextStream s( &info );

	s << "<b>" << tr("Locale:") << "</b> ";
	QLocale::Language language = QLocale::system().language();
	QString locale = (language == QLocale::C ? "English/United States" : QLocale::languageToString( language ) );
	CPINFOEX CPInfoEx;
	if( GetCPInfoEx( GetConsoleCP(), 0, &CPInfoEx ) != 0 )
		locale.append(" / ").append(QString::fromWCharArray(CPInfoEx.CodePageName));
	s << locale << "<br />";
	emit update( info );
	info.clear();

	s << "<b>" << tr("User rights: ") << "</b>" << getUserRights() << "<br />";
	emit update( info );
	info.clear();

	QStringList base = Common::packages({
		"Eesti ID-kaardi tarkvara", "Estonian ID-card software", "eID software"}, false);
	if( !base.isEmpty() )
		s << "<b>" << tr("Base version:") << "</b> " << base.join( "<br />" ) << "<br />";
	s << "<b>" << tr("Application version:") << "</b> " << QCoreApplication::applicationVersion() << " (" << QSysInfo::WordSize << " bit)<br />";
	emit update( info );
	info.clear();

	s << "<b>" << tr("OS:") << "</b> " << Common::applicationOs() << "<br /><br /";
	emit update( info );
	info.clear();

	s << "<b>" << tr("Libraries") << ":</b><br />" << "QT (" << qVersion() << ")<br />";

	QByteArray path = qgetenv("PATH");
	qputenv("PATH", path
		+ ";C:\\Program Files\\Open-EID"
		+ ";C:\\Program Files\\TeRa Client"
		+ ";C:\\Program Files\\EstIDMinidriver Minidriver"
		+ ";C:\\Program Files (x86)\\Open-EID"
		+ ";C:\\Program Files (x86)\\TeRa Client"
		+ ";C:\\Program Files (x86)\\EstIDMinidriver Minidriver");
	SetDllDirectory(LPCWSTR(qApp->applicationDirPath().utf16()));
	static const QStringList packages{
		"digidoc", "digidocpp", "qdigidoc4.exe", "qdigidocclient.exe", "qesteidutil.exe", "id-updater.exe", "qdigidoc_tera_gui.exe",
		"esteidcm", "esteidcm64", "EstIDMinidriver", "EstIDMinidriver64", "onepin-opensc-pkcs11", "EsteidShellExtension",
		"esteid-plugin-ie", "esteid-plugin-ie64", "chrome-token-signing.exe",
		"zlib1", "libeay32", "ssleay32", "libcrypto-1_1", "libssl-1_1", "libcrypto-1_1-x64", "libssl-1_1-x64", "xerces-c_3_1", "xerces-c_3_2", "xsec_1_7", "libxml2",
		"advapi32", "crypt32", "winscard"};
	for(const QString &lib: packages)
	{
		DWORD infoHandle = 0;
		DWORD sz = GetFileVersionInfoSize(LPCWSTR(lib.utf16()), &infoHandle);
		if( !sz )
			continue;
		QByteArray data(int(sz * 2), 0);
		if( !GetFileVersionInfoW( LPCWSTR(lib.utf16()), 0, sz, data.data() ) )
			continue;
		VS_FIXEDFILEINFO *info = nullptr;
		UINT len = 0;
		if( !VerQueryValueW( data.constData(), L"\\", (LPVOID*)&info, &len ) )
			continue;
		s << QString( "%1 (%2.%3.%4.%5)" ).arg( lib )
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

	enum {
		Running,
		Stopped,
		NotFound
	} atrfiltr = NotFound, certprop = NotFound;
	if(SC_HANDLE h = OpenSCManager(nullptr, nullptr, SC_MANAGER_CONNECT))
	{
		if( SC_HANDLE s = OpenService( h, L"atrfiltr", SERVICE_QUERY_STATUS ) )
		{
			SERVICE_STATUS status;
			QueryServiceStatus( s, &status );
			atrfiltr = (status.dwCurrentState == SERVICE_RUNNING) ? Running : Stopped;
			CloseServiceHandle( s );
		}
		if( SC_HANDLE s = OpenService( h, L"CertPropSvc", SERVICE_QUERY_STATUS ))
		{
			SERVICE_STATUS status;
			QueryServiceStatus( s, &status );
			certprop = (status.dwCurrentState == SERVICE_RUNNING) ? Running : Stopped;
			CloseServiceHandle( s );
		}
		CloseServiceHandle( h );
	}
	s << "<br /><b>" << tr("ATRfiltr service status: ") << "</b>" << " ";
	switch( atrfiltr )
	{
	case NotFound: s << tr("Not found"); break;
	case Stopped: s << tr("Not running"); break;
	case Running: s << tr("Running"); break;
	}
	s << "<br /><b>" << tr("Certificate Propagation service status: ") << "</b>" << " ";
	switch( certprop )
	{
	case NotFound: s << tr("Not found"); break;
	case Stopped: s << tr("Not running"); break;
	case Running: s << tr("Running"); break;
	}
	s << "<br />";

	generalInfo( s );
	emit update( info );
	info.clear();

	QStringList browsers = Common::packages( QStringList() << "Mozilla Firefox" << "Google Chrome" );
	QSettings reg( "HKEY_LOCAL_MACHINE\\SOFTWARE\\Microsoft\\Internet Explorer", QSettings::NativeFormat );
	browsers << QString( "Internet Explorer (%1)" ).arg(
		reg.value("svcVersion", reg.value( "Version" ) ).toString() );
	s << "<br /><br /><b>" << tr("Browsers:") << "</b><br />" << browsers.join( "<br />" ) << "<br /><br />";
	emit update( info );
	info.clear();
}
