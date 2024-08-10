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

#include <QtCore/QFile>
#include <QtCore/QRegularExpression>
#include <QtCore/QTextStream>
#include <QtCore/QProcess>
#include <QtNetwork/QSslSocket>

#include <sys/utsname.h>
#ifdef Q_OS_DARWIN
#include <CoreFoundation/CFBundle.h>
#endif

QStringList Diagnostics::packages(const QStringList &names, bool withName)
{
	QStringList packages;
#ifdef Q_OS_DARWIN
	Q_UNUSED(withName);
	for (const QString &name: names) {
		CFStringRef id = QStringLiteral("ee.ria.%1").arg(name).toCFString();
		CFBundleRef bundle = CFBundleGetBundleWithIdentifier(id);
		CFRelease(id);
		if (!bundle)
			continue;
		CFStringRef ver = CFStringRef(CFBundleGetValueForInfoDictionaryKey(bundle, CFSTR("CFBundleShortVersionString")));
		CFStringRef build = CFStringRef(CFBundleGetValueForInfoDictionaryKey(bundle, CFSTR("CFBundleVersion")));
		packages.append(QStringLiteral("%1 (%2.%3)").arg(name, QString::fromCFString(ver), QString::fromCFString(build)));
	}
#elif defined(Q_OS_LINUX)
	QProcess p;

	for(const QString &name: names)
	{
		p.start("dpkg-query", { "-W", "-f=${Version}", name });
		if( !p.waitForStarted() && p.error() == QProcess::FailedToStart )
		{
			p.start("rpm", { "-q", "--qf", "%{VERSION}", name });
			p.waitForStarted();
		}
		p.waitForFinished();
		if( !p.exitCode() )
		{
			QString ver = QString::fromLocal8Bit( p.readAll().trimmed() );
			if( !ver.isEmpty() )
				packages.append(packageName(name, ver, withName));
		}
	}
#endif
	return packages;
}

void Diagnostics::run()
{
	QString info;
	QTextStream s( &info );

	QLocale::Language language = QLocale::system().language();
	QString ctype = QProcessEnvironment::systemEnvironment().value(QStringLiteral("LC_CTYPE"),
		QProcessEnvironment::systemEnvironment().value(QStringLiteral("LANG")));
	s << "<b>" << tr("Locale:") << "</b> "
		<< (language == QLocale::C ? QStringLiteral("English/United States") : QLocale::languageToString(language));
	if( !ctype.isEmpty() )
	{
		s << " / " << ctype;
	}
	s << "<br /><br />";
	emit update( info );
	info.clear();

#ifndef Q_OS_DARWIN
	QStringList package = packages({"open-eid"}, false);
	if( !package.isEmpty() )
		s << "<b>" << tr("Base version:") << "</b> " << package.first() << "<br />";
#endif
	s << "<b>" << tr("Application version:") << "</b> " << QCoreApplication::applicationVersion() << " (" << QSysInfo::WordSize << " bit)<br />";
	emit update( info );
	info.clear();

	s << "<b>" << tr("OS:") << "</b> " << Common::applicationOs() << "<br />";
#ifndef Q_OS_DARWIN
	s << "<b>" << tr("CPU:") << "</b> ";
	QFile f( "/proc/cpuinfo" );
	if( f.open( QFile::ReadOnly ) )
	{
		QRegularExpression rx(QStringLiteral("model name.*\\: (.*)\n"));
		rx.setPatternOptions(QRegularExpression::InvertedGreedinessOption);
		QRegularExpressionMatch match = rx.match(QString::fromLocal8Bit(f.readAll()));
		if(match.hasMatch())
			s << match.captured(1);
	}
	s << "<br />";
#endif
	struct utsname unameData = {};
	uname(&unameData);
	s << "<b>" << tr("Kernel:") << "</b> "
		<< unameData.sysname << " " << unameData.release << " "
		<< unameData.version << " " << unameData.machine << "<br /><br />";
	emit update( info );
	info.clear();

	s << "<b>" << tr("Libraries") << ":</b><br />"
		<< "QT (" << qVersion() << ")" << "<br />"
		<< "OpenSSL build (" << QSslSocket::sslLibraryBuildVersionString() << ")<br />"
		<< "OpenSSL current (" << QSslSocket::sslLibraryVersionString() << ")<br />"
		<< packages({
#ifdef Q_OS_DARWIN
		"digidocpp"
#else
		"libdigidocpp1", "qdigidoc4", "firefox-pkcs11-loader", "chrome-token-signing", "web-eid",
		"libxml2", "libxmlsec1", "libpcsclite1", "pcsc-lite", "opensc"
#endif
		}).join(QStringLiteral("<br />")) << "<br /><br />";
	emit update( info );
	info.clear();

	generalInfo(s);
	emit update( info );
	info.clear();

#ifndef Q_OS_DARWIN
	QStringList browsers = packages({"chromium-browser", "firefox", "MozillaFirefox", "google-chrome-stable"});
	if( !browsers.isEmpty() )
		s << "<br /><br /><b>" << tr("Browsers:") << "</b><br />" << browsers.join(QStringLiteral("<br />")) << "<br /><br />";
	emit update( info );
	info.clear();

	QProcess p;
	p.start(QStringLiteral("lsusb"), QStringList());
	p.waitForFinished();
	QString cmd = QString::fromLocal8Bit( p.readAll() );
	if( !cmd.isEmpty() )
		s << "<b>" << tr("USB info:") << "</b><br/> " << cmd.replace( "\n", "<br />" ) << "<br />";
	emit update( info );
	info.clear();
#endif
}
