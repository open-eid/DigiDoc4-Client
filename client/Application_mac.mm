// SPDX-FileCopyrightText: Estonian Information System Authority
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "Application.h"

#include <Cocoa/Cocoa.h>
#include <QtCore/QTextStream>
#include <QtCore/QUrl>
#include <QtCore/QUrlQuery>
#include <QtGui/QDesktopServices>

using namespace Qt::StringLiterals;

static auto fetchPaths(NSPasteboard *pboard)
{
	QStringList result;
	NSArray<NSURL *> *urls = [pboard readObjectsForClasses:@[[NSURL class]]
		options:@{ NSPasteboardURLReadingFileURLsOnlyKey : @YES }];
	for (NSURL *url in urls) {
		result.append(QString::fromNSString(url.path).normalized(QString::NormalizationForm_C));
	}
	return result;
}

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
	Application::showClient(fetchPaths(pboard));
}

- (void)signClient:(NSPasteboard *)pboard userData:(NSString *)data error:(NSString **)error
{
	Q_UNUSED(data)
	Q_UNUSED(error)
	Application::showClient(fetchPaths(pboard), false, true);
}

- (void)openCrypto:(NSPasteboard *)pboard userData:(NSString *)data error:(NSString **)error
{
	Q_UNUSED(data)
	Q_UNUSED(error)
	Application::showClient(fetchPaths(pboard), true);
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
	if(NSURL *appUrl = [NSWorkspace.sharedWorkspace URLForApplicationToOpenURL:url.toNSURL()])
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
		else if([appUrl.path rangeOfString:@"Outlook"].location != NSNotFound)
		{
			s << "on run" << Qt::endl
			<< "set vattachment to \"" << q.queryItemValue(u"attachment"_s) << "\" as POSIX file" << Qt::endl
			<< "set vsubject to \"" << q.queryItemValue(u"subject"_s) << "\"" << Qt::endl
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

QString Application::groupContainerPath()
{
	return QString::fromNSString([NSFileManager.defaultManager
		containerURLForSecurityApplicationGroupIdentifier:@"group.ee.ria.qdigidoc4.tsl"].path);
}

