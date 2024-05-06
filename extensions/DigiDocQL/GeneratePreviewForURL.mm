/*
 * QEstEidClient
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

// https://developer.apple.com/library/archive/documentation/UserExperience/Conceptual/Quicklook_Programming_Guide/Introduction/Introduction.html

#include <digidocpp/Container.h>
#include <digidocpp/DataFile.h>
#include <digidocpp/Signature.h>
#include <digidocpp/Exception.h>
#include <digidocpp/XmlConf.h>

#include <Foundation/Foundation.h>
#include <QuickLook/QuickLook.h>

using namespace digidoc;

QL_EXTERN_C_BEGIN
OSStatus GeneratePreviewForURL(void *thisInterface, QLPreviewRequestRef preview,
	CFURLRef url, CFStringRef contentTypeUTI, CFDictionaryRef options);
void CancelPreviewGeneration(void * /*thisInterface*/, QLPreviewRequestRef /*preview*/) {}
QL_EXTERN_C_END

@interface NSString (Digidoc)
+ (NSString*)stdstring:(const std::string&)str;
+ (NSString*)fileSize:(unsigned long)bytes;
+ (void)parseException:(const Exception&)e result:(NSMutableArray *)result;
@end

@implementation NSString (Digidoc)
+ (NSString*)stdstring:(const std::string&)str
{
	return str.empty() ? [NSString string] : [NSString stringWithUTF8String:str.c_str()];
}

+ (NSString*)htmlEntityEncode:(NSString*)str
{
	str = [str stringByReplacingOccurrencesOfString:@"&" withString:@"&amp;"];
	str = [str stringByReplacingOccurrencesOfString:@"\"" withString:@"&quot;"];
	str = [str stringByReplacingOccurrencesOfString:@"'" withString:@"&apos;"];
	str = [str stringByReplacingOccurrencesOfString:@"<" withString:@"&lt;"];
	str = [str stringByReplacingOccurrencesOfString:@">" withString:@"&gt;"];
	return str;
}

+ (NSString*)fileSize:(unsigned long)bytes
{
	enum {
		kb = 1UL << 1,
		mb = 1UL << 2,
		gb = 1UL << 3
	};
	if (bytes >= gb)
		return [NSString stringWithFormat:@"%1.2f GB", double(bytes) / gb];
	if (bytes >= mb)
		return [NSString stringWithFormat:@"%1.2f MB", double(bytes) / mb];
	if (bytes >= kb)
		return [NSString stringWithFormat:@"%1.1f KB", double(bytes) / kb];
	return [NSString stringWithFormat:@"%lu bytes", bytes];
}

+ (void)parseException:(const Exception&)e result:(NSMutableArray *)result
{
	[result addObject:[self stdstring:e.msg()]];
	for (const Exception &i : e.causes()) {
		[self parseException:i result:result];
	}
}
@end

class DigidocConf: public digidoc::XmlConfCurrent
{
public:
	bool TSLAutoUpdate() const final { return false; }
	bool TSLOnlineDigest() const final { return false; }
	std::string TSLCache() const final
	{
		std::string home = "~";
		if(char *var = getenv("HOME"))
			home = var;
		return home + "/Library/Containers/ee.ria.qdigidoc4/Data/Library/Application Support/RIA/qdigidoc4/";
	}
};

OSStatus GeneratePreviewForURL(void */*thisInterface*/, QLPreviewRequestRef preview,
	CFURLRef url, CFStringRef /*contentTypeUTI*/, CFDictionaryRef /*options*/)
{
	@autoreleasepool {
	NSMutableString *h = [NSMutableString string];
	[h appendString:@"<html><head><style>"];
	[h appendString:@"* { font-family: 'Lucida Sans Unicode', 'Lucida Grande', sans-serif };"];
	[h appendString:@"body { font-size: 10pt };"];
	[h appendString:@"h2 { padding-left: 50px; background: url(cid:asic.icns); background-size: 42px 42px; background-repeat:no-repeat; };"];
	[h appendString:@"font, dt { color: #808080 };"];
	[h appendString:@"dt { float: left; clear: left; margin-left: 30px; margin-right: 10px };"];
	[h appendString:@"dl { margin-bottom: 10px };"];
	[h appendString:@"</style></head><body>"];
	[h appendFormat:@"<h2>%@<hr size='1' /></h2>", [NSString htmlEntityEncode:[(__bridge NSURL*)url lastPathComponent]]];
	try
	{
		digidoc::Conf::init( new DigidocConf );
		digidoc::initialize();
		std::unique_ptr<Container> d(Container::openPtr([(__bridge NSURL*)url path].UTF8String));

		[h appendString:@"<font>Files</font><ol>"];
		for (const DataFile *doc : d->dataFiles()) {
			[h appendFormat:@"<li>%@</li>", [NSString htmlEntityEncode:[NSString stdstring:doc->fileName()]]];
		}
		[h appendString:@"</ol>"];

		[h appendString:@"<font>Signatures</font>"];
		for (const Signature *s : d->signatures()) {
			[h appendFormat:@"<dl><dt>Signer</dt><dd>%@</dd>", [NSString htmlEntityEncode:[NSString stdstring:s->signedBy()]]];

			NSString *date = [NSString stdstring:s->trustedSigningTime()];
			[date stringByReplacingOccurrencesOfString:@"Z" withString:@"-0000"];
			NSDateFormatter *df = [[NSDateFormatter alloc] init];
			[df setDateFormat:@"yyyy-MM-dd'T'HH:mm:ssZ"];
			NSDate *formateddate = [df dateFromString:date];
			[df setTimeZone: [NSTimeZone defaultTimeZone]];
			[df setDateFormat:@"YYYY-MM-dd HH:mm:ss z"];
			[h appendFormat:@"<dt>Time</dt><dd>%@</dd>", [df stringFromDate:formateddate]];

			Signature::Validator v(s);
			NSString *status = @"not valid";
			switch(v.status())
			{
			case Signature::Validator::Valid: status = @"valid"; break;
			case Signature::Validator::Warning: status = @"valid with warnings"; break;
			case Signature::Validator::NonQSCD: status = @"valid with limitations"; break;
			case Signature::Validator::Test: status = @"valid test signature"; break;
			case Signature::Validator::Invalid: status = @"invalid"; break;
			case Signature::Validator::Unknown: status = @"unknown"; break;
			}
			[h appendFormat:@"<dt>Validity</dt><dd>Signature is %@</dd>", status];

			NSMutableArray *roles = [NSMutableArray array];
			for (const std::string &role : s->signerRoles()) {
				if( !role.empty() ) {
					[roles addObject:[NSString htmlEntityEncode:[NSString stdstring:role]]];
				}
			}
			if( [roles count] > 0 ) {
				[h appendFormat:@"<dt>Role</dt><dd>%@&nbsp;</dd>", [NSString htmlEntityEncode:[roles componentsJoinedByString:@" / "]]];
			}
			if (!s->countryName().empty()) {
				[h appendFormat:@"<dt>Country</dt><dd>%@&nbsp;</dd>", [NSString htmlEntityEncode:[NSString stdstring:s->countryName()]]];
			}
			if (!s->city().empty()) {
				[h appendFormat:@"<dt>City</dt><dd>%@&nbsp;</dd>", [NSString htmlEntityEncode:[NSString stdstring:s->city()]]];
			}
			if (!s->stateOrProvince().empty()) {
				[h appendFormat:@"<dt>State</dt><dd>%@&nbsp;</dd>", [NSString htmlEntityEncode:[NSString stdstring:s->stateOrProvince()]]];
			}
			if (!s->postalCode().empty()) {
				[h appendFormat:@"<dt>Postal code</dt><dd>%@&nbsp;</dd>", [NSString htmlEntityEncode:[NSString stdstring:s->postalCode()]]];
			}
			[h appendString:@"</dl>"];
		}
		digidoc::terminate();
	} catch (const Exception &e) {
		NSMutableArray *err = [NSMutableArray array];
		[NSString parseException:e result:err];
		[h appendFormat:@"Failed to load document:<br />%@", [err componentsJoinedByString:@"<br />"]];
	}
	[h appendString:@"</body></html>"];

	NSBundle *bundle = [NSBundle bundleWithIdentifier:@"ee.ria.DigiDocQL"];
	NSData *image = [NSData dataWithContentsOfFile:[bundle pathForResource:@"asic" ofType:@"icns"]];
	NSDictionary *props = @{
		(__bridge id)kQLPreviewPropertyTextEncodingNameKey : @"UTF-8",
		(__bridge id)kQLPreviewPropertyMIMETypeKey : @"text/html",
		(__bridge id)kQLPreviewPropertyWidthKey : [[bundle infoDictionary] valueForKey:@"QLPreviewWidth"],
		(__bridge id)kQLPreviewPropertyHeightKey : [[bundle infoDictionary] valueForKey:@"QLPreviewHeight"],
		(__bridge id)kQLPreviewPropertyAttachmentsKey : @{
			@"asic.icns" : @{
				(__bridge id)kQLPreviewPropertyMIMETypeKey : @"image/icns",
				(__bridge id)kQLPreviewPropertyAttachmentDataKey : image
			}
		}
	};
	QLPreviewRequestSetDataRepresentation(preview,
		(__bridge CFDataRef)[h dataUsingEncoding:NSUTF8StringEncoding], kUTTypeHTML, (__bridge CFDictionaryRef)props);
	}
	return noErr;
}
