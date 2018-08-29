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

#include "Application.h"

#include <Cocoa/Cocoa.h>
#include <QtCore/QTextStream>
#include <QtCore/QUrl>
#include <QtCore/QUrlQuery>
#include <QtGui/QDesktopServices>

@implementation NSApplication (ApplicationObjC)

- (void)appReopen:(NSAppleEventDescriptor *)event withReplyEvent:(NSAppleEventDescriptor *)replyEvent
{
	Q_UNUSED(event)
	Q_UNUSED(replyEvent)
	QApplication::postEvent( qApp, new REOpenEvent );
}

- (void)openClient:(NSPasteboard *)pboard userData:(NSString *)data error:(NSString **)error
{
	Q_UNUSED(data)
	Q_UNUSED(error)
	QStringList result;
	for( NSString *filename in [pboard propertyListForType:NSFilenamesPboardType] )
		result << QString::fromNSString( filename );
	QMetaObject::invokeMethod( qApp, "showClient", Q_ARG(QStringList,result) );
}

- (void)signClient:(NSPasteboard *)pboard userData:(NSString *)data error:(NSString **)error
{
	Q_UNUSED(data)
	Q_UNUSED(error)
	QStringList result;
	for(NSString *filename in [pboard propertyListForType:NSFilenamesPboardType])
		result << QString::fromNSString(filename);
	QMetaObject::invokeMethod(qApp, "showClient", Q_ARG(QStringList,result), Q_ARG(bool,false), Q_ARG(bool,true));
}

- (void)openCrypto:(NSPasteboard *)pboard userData:(NSString *)data error:(NSString **)error
{
	Q_UNUSED(data)
	Q_UNUSED(error)
	QStringList result;
	for( NSString *filename in [pboard propertyListForType:NSFilenamesPboardType] )
		result << QString::fromNSString( filename );
	QMetaObject::invokeMethod( qApp, "showClient", Q_ARG(QStringList,result), Q_ARG(bool,true) );
}
@end

void Application::addRecent( const QString &file )
{
	if( !file.isEmpty() )
		[[NSDocumentController sharedDocumentController] noteNewRecentDocumentURL:[NSURL fileURLWithPath:file.toNSString()]];
}

void Application::initMacEvents()
{
	[[NSAppleEventManager sharedAppleEventManager] setEventHandler:NSApp
		andSelector:@selector(appReopen:withReplyEvent:)
		forEventClass:kCoreEventClass
		andEventID:kAEReopenApplication];
	// reload /System/Library/CoreServices/pbs
	// list /System/Library/CoreServices/pbs -dump_pboard
	[NSApp setServicesProvider:NSApp];
}

void Application::deinitMacEvents()
{
	[[NSAppleEventManager sharedAppleEventManager]
		removeEventHandlerForEventClass:kCoreEventClass
		andEventID:kAEReopenApplication];
}

void Application::mailTo( const QUrl &url )
{
	QUrlQuery q(url);
	if(CFURLRef appUrl = LSCopyDefaultApplicationURLForURL((__bridge CFURLRef)url.toNSURL(), kLSRolesAll, nil))
	{
		NSString *appPath = [((__bridge NSURL *)appUrl) path];
		CFRelease( appUrl );
		QString p;
		QTextStream s( &p );
		if( [appPath rangeOfString:@"/Applications/Mail.app"].location != NSNotFound )
		{
			s << "on run" << endl
			<< "set vattachment to \"" << q.queryItemValue("attachment") << "\"" << endl
			<< "set vsubject to \"" << q.queryItemValue("subject") << "\"" << endl
			<< "tell application \"Mail\"" << endl
			<< "set msg to make new outgoing message with properties {subject:vsubject, visible:true}" << endl
			<< "tell content of msg to make new attachment with properties {file name:(vattachment as POSIX file) as alias}" << endl
			<< "activate" << endl
			<< "end tell" << endl
			<< "end run" << endl;
		}
		else if( [appPath rangeOfString:@"Entourage"].location != NSNotFound )
		{
			s << "on run" << endl
			<< "set vattachment to \"" << q.queryItemValue("attachment") << "\"" << endl
			<< "set vsubject to \"" << q.queryItemValue("subject") << "\"" << endl
			<< "tell application \"Microsoft Entourage\"" << endl
			<< "set vmessage to make new outgoing message with properties" << endl
			<< "{subject:vsubject, attachments:vattachment}" << endl
			<< "open vmessage" << endl
			<< "activate" << endl
			<< "end tell" << endl
			<< "end run" << endl;
		}
		else if( [appPath rangeOfString:@"Outlook"].location != NSNotFound )
		{
			s << "on run" << endl
			<< "set vattachment to \"" << q.queryItemValue("attachment") << "\" as POSIX file" << endl
			<< "set vsubject to \"" << q.queryItemValue("subject") << "\"" << endl
			<< "tell application \"Microsoft Outlook\"" << endl
			<< "activate" << endl
			<< "set vmessage to make new outgoing message with properties {subject:vsubject}" << endl
			<< "make new attachment at vmessage with properties {file: vattachment}" << endl
			<< "open vmessage" << endl
			<< "end tell" << endl
			<< "end run" << endl;
		}
#if 0
		else if([appPath rangeOfString:"/Applications/Thunderbird.app"].location != NSNotFound)
		{
			// TODO: Handle Thunderbird here? Impossible?
		}
#endif
		if(!p.isEmpty())
		{
			NSAppleScript *appleScript = [[NSAppleScript alloc] initWithSource:p.toNSString()];
			NSDictionary *err;
			if([appleScript executeAndReturnError:&err])
				return;
		}
	}
	QDesktopServices::openUrl( url );
}
