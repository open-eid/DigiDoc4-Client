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

#include "MacUtil.h"
#include <Foundation/Foundation.h>
#include <AppKit/AppKit.h>

#include <common/Settings.h>
#include <QByteArray>
#include <string>

namespace ria
{
namespace qdigidoc4
{

NSData* toNSData(const QByteArray& data)
{
	return [NSData dataWithBytes:data.constData() length:data.size()];
}

QByteArray toByteArray(const NSData *data)
{
	QByteArray ba;
	ba.resize([data length]);
	[data getBytes:ba.data() length:ba.size()];
	return ba;
}

void MacUtil::bookmark(char const* path)
{
	std::string url;
	url.append("file://");
	url.append(path);
	
	char const* url_path = url.c_str();
	NSString *folder = [NSString stringWithUTF8String:url_path];
	NSURL *fileURL = [NSURL URLWithString:folder];

	NSError *error = nil;
	NSData *bookmark = [fileURL bookmarkDataWithOptions:NSURLBookmarkCreationWithSecurityScope
			includingResourceValuesForKeys:nil
							relativeToURL:nil
									error:&error];
	if (error)
	{
		NSLog(@"MacUtil - Error creating bookmark for URL (%@): %@", folder, error);
	}
	else
	{
		NSLog(@"MacUtil - Created bookmark for URL (%@)", folder);
		Settings settings;
		QVariantMap bookmarks = settings.value("BookmarkedFolders").toMap();
		bookmarks[url_path] = toByteArray(bookmark);
		settings.setValue("BookmarkedFolders", bookmarks);
	}
}

bool MacUtil::isWritable(char const* path)
{
	// https://stackoverflow.com/questions/12153504/accessing-the-desktop-in-a-sandboxed-app
	std::string url;
	url.append("file://");
	url.append(path);

	char const* url_path = url.c_str();

	NSString *folder = [NSString stringWithUTF8String:url_path];
	NSURL *bookmarkedURL = [NSURL URLWithString:folder];
	BOOL ok = [bookmarkedURL startAccessingSecurityScopedResource];
	NSLog(@"MacUtil - Accessed ok: %d URL (%@)", ok, folder);

	if(!ok)
	{
		Settings settings;
		QVariantMap bookmarks = settings.value("BookmarkedFolders").toMap();
		if(bookmarks.contains(url_path))
		{
			NSData *bookmark = toNSData(bookmarks[url_path].toByteArray());
			NSError *error = nil;
			bookmarkedURL = [NSURL URLByResolvingBookmarkData:bookmark options:NSURLBookmarkResolutionWithSecurityScope
				relativeToURL:nil bookmarkDataIsStale:nil error:&error];
			ok = [bookmarkedURL startAccessingSecurityScopedResource];
			NSLog(@"MacUtil - Accessed bookmark ok: %d URL (%@)", ok, folder);
		}
	}

	return ok;
}

}; // namespace qdigidoc4
}; // namespace ria
