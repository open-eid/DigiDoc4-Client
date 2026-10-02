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

#include <digidocpp/Container.h>
#include <digidocpp/DataFile.h>
#include <digidocpp/Signature.h>
#include <digidocpp/Exception.h>
#include <digidocpp/Conf.h>
#import <QuickLookUI/QuickLookUI.h>

#include <string_view>

using namespace digidoc;
 
class DigidocConf final: public digidoc::ConfCurrent
{
public:
    bool TSLAllowExpired() const final { return true; }
    bool TSLAutoUpdate() const final { return false; }
    bool TSLOnlineDigest() const final { return false; }
    std::string TSLCache() const final
    {
        NSURL *directory = [NSFileManager.defaultManager containerURLForSecurityApplicationGroupIdentifier:@"group.ee.ria.qdigidoc4.tsl"];
        const char *path = directory.path.fileSystemRepresentation;
        return path ? path : "";
    }
};

@implementation NSString (Digidoc)
+ (NSString*)fromUTF8:(std::string_view)value
{
    if (value.empty())
        return @"";
    NSString *result = [[NSString alloc] initWithBytes:value.data() length:value.size() encoding:NSUTF8StringEncoding];
    if (result)
        return result;
    result = [[NSString alloc] initWithBytes:value.data() length:value.size() encoding:NSISOLatin1StringEncoding];
    return result ?: @"";
}

+ (NSString*)fileSize:(unsigned long)bytes
{
    if (const auto gb = 1UL << 3; bytes >= gb)
        return [NSString stringWithFormat:@"%1.2f GB", double(bytes) / double(gb)];
    if (const auto mb = 1UL << 2; bytes >= mb)
        return [NSString stringWithFormat:@"%1.2f MB", double(bytes) / double(mb)];
    if (const auto kb = 1UL << 1; bytes >= kb)
        return [NSString stringWithFormat:@"%1.1f KB", double(bytes) / double(kb)];
    return [NSString stringWithFormat:@"%lu bytes", bytes];
}

+ (void)parseException:(const Exception&)e result:(NSMutableArray *)result
{
    [result addObject:[self htmlEntityEncode:e.msg()]];
    for (const Exception &i : e.causes()) {
        [self parseException:i result:result];
    }
}

+ (NSString*)htmlEntityEncodeString:(NSString*)value
{
    NSMutableString *result = [NSMutableString stringWithString:value ?: @""];
    for (NSArray<NSString*> *replacement in @[
        @[@"&", @"&amp;"],
        @[@"\"", @"&quot;"],
        @[@"'", @"&apos;"],
        @[@"<", @"&lt;"],
        @[@">", @"&gt;"]
    ]) {
        [result replaceOccurrencesOfString:replacement[0] withString:replacement[1]
            options:0 range:NSMakeRange(0, result.length)];
    }
    return result;
}

+ (NSString*)htmlEntityEncode:(std::string_view)value
{
    return [self htmlEntityEncodeString:[self fromUTF8:value]];
}
@end

@interface PreviewProvider : QLPreviewProvider<QLPreviewingController>
@end

@implementation PreviewProvider
- (void)providePreviewForFileRequest:(QLFilePreviewRequest *)request
                   completionHandler:(void (^)(QLPreviewReply *reply, NSError *error))handler
{
    QLPreviewReply* reply = [[QLPreviewReply alloc] initWithDataOfContentType:UTTypeHTML contentSize:CGSizeMake(800, 800)
        dataCreationBlock:^NSData* (QLPreviewReply *replyToUpdate, NSError **error) {
        replyToUpdate.stringEncoding = NSUTF8StringEncoding;
        NSURL *iconURL = [NSBundle.mainBundle URLForResource:@"asic" withExtension:@"icns"];
        if (NSData *iconData = [NSData dataWithContentsOfURL:iconURL]) {
            replyToUpdate.attachments = @{ @"asic.icns":
                [[QLPreviewReplyAttachment alloc] initWithData:iconData contentType:UTTypeICNS] };
        }
        NSISO8601DateFormatter *dfrom = [[NSISO8601DateFormatter alloc] init];
        NSDateFormatter *dto = [[NSDateFormatter alloc] init];
        [dto setTimeZone: [NSTimeZone defaultTimeZone]];
        [dto setDateFormat:@"YYYY-MM-dd HH:mm:ss z"];

        NSMutableString *h = [NSMutableString stringWithString:@R"(
<html><head><meta charset="UTF-8"><style>
:root { color-scheme: light dark; }
* { font-family: -apple-system, system-ui, sans-serif; }
body { font-size: 10pt; }
h2 img { width: 42px; height: 42px; vertical-align: middle; margin-right: 8px; }
font, dt { color: #808080; }
dt { float: left; clear: left; margin-left: 30px; margin-right: 10px; }
dl { margin-bottom: 10px; }
@media (prefers-color-scheme: dark) {
    font, dt { color: #a8a8a8; }
}
</style></head><body>)"];
        [h appendFormat:@"<h2><img src=\"cid:asic.icns\" />%@<hr size='1' /></h2>",
            [NSString htmlEntityEncodeString:request.fileURL.lastPathComponent]];
        try
        {
            static const bool initialized = [] {
                digidoc::Conf::init(new DigidocConf());
                digidoc::initialize();
                atexit(&digidoc::terminate);
                return true;
            }();

            struct ValidateOnline: public ContainerOpenCB
            {
                bool validateOnline() const { return false; }
            } validateOnline;
            std::unique_ptr<Container> d = Container::openPtr(request.fileURL.path.UTF8String, &validateOnline);

            [h appendString:@"<font>Files</font><ol>"];
            for (const DataFile *doc : d->dataFiles()) {
                [h appendFormat:@"<li>%@</li>", [NSString htmlEntityEncode:doc->fileName()]];
            }
            [h appendString:@"</ol>"];

            [h appendString:@"<font>Signatures</font>"];
            for (const Signature *s : d->signatures()) {
                [h appendFormat:@"<dl><dt>Signer</dt><dd>%@</dd>", [NSString htmlEntityEncode:s->signedBy()]];

                NSString *date = [NSString fromUTF8:s->trustedSigningTime()];
                [h appendFormat:@"<dt>Time</dt><dd>%@</dd>", [dto stringFromDate:[dfrom dateFromString:date]]];

                Signature::Validator v(s);
                NSString *status = @"not valid";
                switch(v.status())
                {
                using enum Signature::Validator::Status;
                case Valid: status = @"valid"; break;
                case Warning: status = @"valid with warnings"; break;
                case NonQSCD: status = @"valid with limitations"; break;
                case Test: status = @"valid test signature"; break;
                case Invalid: status = @"invalid"; break;
                case Unknown: status = @"unknown"; break;
                }
                [h appendFormat:@"<dt>Validity</dt><dd>Signature is %@</dd>", status];

                std::string roles;
                for (const std::string &role : s->signerRoles()) {
                    if(role.empty())
                        continue;
                    if(!roles.empty())
                        roles += " / ";
                    roles += role;
                }
                if (!roles.empty()) {
                    [h appendFormat:@"<dt>Role</dt><dd>%@&nbsp;</dd>", [NSString htmlEntityEncode:roles]];
                }
                if (!s->countryName().empty()) {
                    [h appendFormat:@"<dt>Country</dt><dd>%@&nbsp;</dd>", [NSString htmlEntityEncode:s->countryName()]];
                }
                if (!s->city().empty()) {
                    [h appendFormat:@"<dt>City</dt><dd>%@&nbsp;</dd>", [NSString htmlEntityEncode:s->city()]];
                }
                if (!s->stateOrProvince().empty()) {
                    [h appendFormat:@"<dt>State</dt><dd>%@&nbsp;</dd>", [NSString htmlEntityEncode:s->stateOrProvince()]];
                }
                if (!s->postalCode().empty()) {
                    [h appendFormat:@"<dt>Postal code</dt><dd>%@&nbsp;</dd>", [NSString htmlEntityEncode:s->postalCode()]];
                }
                [h appendString:@"</dl>"];
            }
        } catch (const Exception &e) {
            NSMutableArray *err = [NSMutableArray array];
            [NSString parseException:e result:err];
            [h appendFormat:@"Failed to load document:<br />%@", [err componentsJoinedByString:@"<br />"]];
        } catch (const std::exception &e) {
            [h appendFormat:@"Failed to load document:<br />%@", [NSString htmlEntityEncode:e.what()]];
        } catch (...) {
            [h appendString:@"Failed to load document:<br />Unknown error"];
        }
        [h appendString:@"</body></html>"];
        return [h dataUsingEncoding:NSUTF8StringEncoding];
    }];
    handler(reply, nil);
}
@end
