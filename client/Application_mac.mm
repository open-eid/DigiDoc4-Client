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
		result.append(QString::fromNSString(filename).normalized(QString::NormalizationForm_C));
	Application::showClient(result);
}

- (void)signClient:(NSPasteboard *)pboard userData:(NSString *)data error:(NSString **)error
{
	Q_UNUSED(data)
	Q_UNUSED(error)
	QStringList result;
	for(NSString *filename in [pboard propertyListForType:NSFilenamesPboardType])
		result.append(QString::fromNSString(filename).normalized(QString::NormalizationForm_C));
	Application::showClient(result, false, true);
}

- (void)openCrypto:(NSPasteboard *)pboard userData:(NSString *)data error:(NSString **)error
{
	Q_UNUSED(data)
	Q_UNUSED(error)
	QStringList result;
	for( NSString *filename in [pboard propertyListForType:NSFilenamesPboardType] )
		result.append(QString::fromNSString(filename).normalized(QString::NormalizationForm_C));
	Application::showClient(result, true);
}
@end

void Application::addRecent( const QString &file )
{
	if( !file.isEmpty() )
		[NSDocumentController.sharedDocumentController noteNewRecentDocumentURL:[NSURL fileURLWithPath:file.toNSString()]];
}

void Application::initMacEvents()
{
	static bool isInitalized = false;
	if(isInitalized)
		return;
	[[NSAppleEventManager sharedAppleEventManager] setEventHandler:NSApp
		andSelector:@selector(appReopen:withReplyEvent:)
		forEventClass:kCoreEventClass
		andEventID:kAEReopenApplication];
	// reload /System/Library/CoreServices/pbs
	// list /System/Library/CoreServices/pbs -dump_pboard
	[NSApp setServicesProvider:NSApp];
	isInitalized = true;
}

void Application::deinitMacEvents()
{
	[NSAppleEventManager.sharedAppleEventManager
		removeEventHandlerForEventClass:kCoreEventClass
		andEventID:kAEReopenApplication];
}

void Application::mailTo( const QUrl &url )
{
	QUrlQuery q(url);
	if(NSURL *appUrl = CFBridgingRelease(LSCopyDefaultApplicationURLForURL((__bridge CFURLRef)url.toNSURL(), kLSRolesAll, nil)))
	{
		QString p;
		QTextStream s( &p );
		if([appUrl.path rangeOfString:@"/Applications/Mail.app"].location != NSNotFound)
		{
			s << "on run" << Qt::endl
			<< "set vattachment to \"" << q.queryItemValue("attachment") << "\"" << Qt::endl
			<< "set vsubject to \"" << q.queryItemValue("subject") << "\"" << Qt::endl
			<< "tell application \"Mail\"" << Qt::endl
			<< "set msg to make new outgoing message with properties {subject:vsubject, visible:true}" << Qt::endl
			<< "tell content of msg to make new attachment with properties {file name:(vattachment as POSIX file) as alias}" << Qt::endl
			<< "activate" << Qt::endl
			<< "end tell" << Qt::endl
			<< "end run" << Qt::endl;
		}
		else if([appUrl.path rangeOfString:@"Entourage"].location != NSNotFound)
		{
			s << "on run" << Qt::endl
			<< "set vattachment to \"" << q.queryItemValue(QStringLiteral("attachment")) << "\"" << Qt::endl
			<< "set vsubject to \"" << q.queryItemValue(QStringLiteral("subject")) << "\"" << Qt::endl
			<< "tell application \"Microsoft Entourage\"" << Qt::endl
			<< "set vmessage to make new outgoing message with properties" << Qt::endl
			<< "{subject:vsubject, attachments:vattachment}" << Qt::endl
			<< "open vmessage" << Qt::endl
			<< "activate" << Qt::endl
			<< "end tell" << Qt::endl
			<< "end run" << Qt::endl;
		}
		else if([appUrl.path rangeOfString:@"Outlook"].location != NSNotFound)
		{
			s << "on run" << Qt::endl
			<< "set vattachment to \"" << q.queryItemValue(QStringLiteral("attachment")) << "\" as POSIX file" << Qt::endl
			<< "set vsubject to \"" << q.queryItemValue(QStringLiteral("subject")) << "\"" << Qt::endl
			<< "tell application \"Microsoft Outlook\"" << Qt::endl
			<< "activate" << Qt::endl
			<< "set vmessage to make new outgoing message with properties {subject:vsubject}" << Qt::endl
			<< "make new attachment at vmessage with properties {file: vattachment}" << Qt::endl
			<< "open vmessage" << Qt::endl
			<< "end tell" << Qt::endl
			<< "end run" << Qt::endl;
		}
#if 0
		else if([appUrl.path rangeOfString:"/Applications/Thunderbird.app"].location != NSNotFound)
		{
			// TODO: Handle Thunderbird here? Impossible?
		}
#endif
		if(!p.isEmpty())
		{
			NSDictionary *err;
			if([[[NSAppleScript alloc] initWithSource:p.toNSString()] executeAndReturnError:&err])
				return;
		}
	}
	QDesktopServices::openUrl( url );
}
