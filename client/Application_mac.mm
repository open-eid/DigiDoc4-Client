// SPDX-FileCopyrightText: Estonian Information System Authority
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "Application.h"

#include <Cocoa/Cocoa.h>
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
	NSSharingService *service = [NSSharingService sharingServiceNamed:NSSharingServiceNameComposeEmail];
	if(service)
	{
		service.subject = q.queryItemValue(u"subject"_s).toNSString();
		NSURL *fileURL = [NSURL fileURLWithPath:q.queryItemValue(u"attachment"_s).toNSString()];
		[service performWithItems:@[fileURL]];
		return;
	}
	QDesktopServices::openUrl(url);
}

QString Application::groupContainerPath()
{
	return QString::fromNSString([NSFileManager.defaultManager
		containerURLForSecurityApplicationGroupIdentifier:@"group.ee.ria.qdigidoc4.tsl"].path);
}

