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

