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

#include "dialogs/FileDialog.h"

#include <QtCore/QFile>
#include <QtCore/QUrl>
#include <QtCore/QUrlQuery>
#include <QtGui/QDesktopServices>

#include <AppKit/AppKit.h>
#include <Security/Security.h>

using namespace Qt::StringLiterals;

// Keeps the attachment's security-scoped access active until the mail composer has
// taken it; the sandbox only lets Mail read a file the application has access to
@interface MailAttachmentDelegate : NSObject <NSSharingServiceDelegate>
- (instancetype)initWithPath:(const QString &)path;
@end

@implementation MailAttachmentDelegate {
	std::unique_ptr<QFile> keepAlive;
	MailAttachmentDelegate *retainSelf; // NSSharingService holds its delegate weakly
}

- (instancetype)initWithPath:(const QString &)path
{
	if(self = [super init])
	{
		keepAlive = FileDialog::keepAccessAlive(path, QFile::ReadOnly);
		retainSelf = self;
	}
	return self;
}

- (void)sharingService:(NSSharingService *)sharingService didShareItems:(NSArray *)items
{
	keepAlive.reset();
	retainSelf = nil;
}

- (void)sharingService:(NSSharingService *)sharingService didFailToShareItems:(NSArray *)items error:(NSError *)error
{
	keepAlive.reset();
	retainSelf = nil;
}
@end

static NSMutableDictionary *proxyCredentialsQuery()
{
	return [@{
		(__bridge id)kSecClass: (__bridge id)kSecClassInternetPassword,
		(__bridge id)kSecAttrProtocol: (__bridge id)kSecAttrProtocolHTTPProxy,
		(__bridge id)kSecAttrSecurityDomain:
			QStringLiteral("%1.proxy").arg(QGuiApplication::desktopFileName()).toNSString()
	} mutableCopy];
}

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

void Application::initMacServices()
{
	// NSServices declared in Info.plist
	// re-register: /System/Library/CoreServices/pbs -flush
	// list:        /System/Library/CoreServices/pbs -dump_pboard
	[NSApp setServicesProvider:NSApp];
}

void Application::mailTo( const QUrl &url )
{
	QUrlQuery q(url);
	NSSharingService *service = [NSSharingService sharingServiceNamed:NSSharingServiceNameComposeEmail];
	if(service)
	{
		QString attachment = q.queryItemValue(u"attachment"_s);
		service.subject = q.queryItemValue(u"subject"_s).toNSString();
		service.delegate = [[MailAttachmentDelegate alloc] initWithPath:attachment];
		NSURL *fileURL = [NSURL fileURLWithPath:attachment.toNSString()];
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

std::optional<Application::ProxyCredentials> Application::proxyCredentials()
{
	NSMutableDictionary *query = proxyCredentialsQuery();
	query[(__bridge id)kSecReturnAttributes] = @YES;
	query[(__bridge id)kSecReturnData] = @YES;
	query[(__bridge id)kSecMatchLimit] = (__bridge id)kSecMatchLimitOne;

	CFTypeRef result = nullptr;
	const OSStatus status = SecItemCopyMatching((__bridge CFDictionaryRef)query, &result);
	if(status == errSecItemNotFound)
		return std::nullopt;
	if(status != errSecSuccess)
	{
		qWarning() << "Failed to read the proxy credentials from Keychain:" << status;
		return std::nullopt;
	}

	NSDictionary *item = CFBridgingRelease(result);
	NSData *data = item[(__bridge id)kSecValueData];
	const quint16 port = [item[(__bridge id)kSecAttrPort] unsignedShortValue];
	return ProxyCredentials {
		QString::fromNSString(item[(__bridge id)kSecAttrServer]),
		port ? QString::number(port) : QString(),
		QString::fromNSString(item[(__bridge id)kSecAttrAccount]),
		QString::fromUtf8(static_cast<const char *>(data.bytes), qsizetype(data.length))
	};
}

bool Application::setProxyCredentials(ProxyCredentials credentials)
{
	NSMutableDictionary *query = proxyCredentialsQuery();
	if(credentials.host.isEmpty() && credentials.port.isEmpty() &&
		credentials.user.isEmpty() && credentials.password.isEmpty())
	{
		const OSStatus status = SecItemDelete((__bridge CFDictionaryRef)query);
		if(status == errSecSuccess || status == errSecItemNotFound)
			return true;
		qWarning() << "Failed to remove the proxy credentials from Keychain:" << status;
		return false;
	}

	QByteArray utf8 = credentials.password.toUtf8();
	auto utf8Scope = qScopeGuard([&] { utf8.fill(0); });
	NSData *data = [NSData dataWithBytesNoCopy:utf8.data()
		length:NSUInteger(utf8.size()) freeWhenDone:NO];
	NSDictionary *attributes = @{
		(__bridge id)kSecAttrServer: credentials.host.toNSString(),
		(__bridge id)kSecAttrPort: @(credentials.port.toUShort()),
		(__bridge id)kSecAttrAccount: credentials.user.toNSString(),
		(__bridge id)kSecValueData: data
	};

	// SecItemUpdate keeps the stored password when the new value is empty,
	// recreate the item to clear it
	OSStatus status = utf8.isEmpty() ? errSecItemNotFound :
		SecItemUpdate((__bridge CFDictionaryRef)query, (__bridge CFDictionaryRef)attributes);
	if(status == errSecItemNotFound)
	{
		SecItemDelete((__bridge CFDictionaryRef)query);
		[query addEntriesFromDictionary:attributes];
		status = SecItemAdd((__bridge CFDictionaryRef)query, nullptr);
	}

	if(status == errSecSuccess)
		return true;
	qWarning() << "Failed to store the proxy credentials in Keychain:" << status;
	return false;
}

Application::ProxyCredentials::ProxyCredentials(
	QString host, QString port, QString user, QString password)
	: host(std::move(host))
	, port(std::move(port))
	, user(std::move(user))
	, password(std::move(password))
{}

Application::ProxyCredentials::~ProxyCredentials()
{
	password.fill(QChar{});
}