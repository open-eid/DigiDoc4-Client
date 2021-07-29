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
#include "QuickLook_mac.h"

#include <QtCore/QFile>
#include <QtCore/QUrl>

#import <objc/runtime.h>
#import <Quartz/Quartz.h>

@interface DocumentPreview : NSResponder <QLPreviewPanelDataSource, QLPreviewPanelDelegate>
@property (nonatomic, copy) NSURL *docUrl;
@end

@implementation DocumentPreview

- (BOOL)acceptsFirstResponder
{
    return YES;
}

- (BOOL)acceptsPreviewPanelControl:(QLPreviewPanel *)panel
{
    Q_UNUSED(panel);
    return YES;
}

- (void)beginPreviewPanelControl:(QLPreviewPanel *)panel
{
    Q_UNUSED(panel);
    panel.dataSource = self;
    panel.delegate = self;
}

- (void)endPreviewPanelControl:(QLPreviewPanel *)panel
{
    Q_UNUSED(panel);
    QFile::remove(QUrl::fromNSURL(self.docUrl).toLocalFile());
    panel.dataSource = nil;
    panel.delegate = nil;
    self.docUrl = nil;
}

- (NSInteger)numberOfPreviewItemsInPreviewPanel:(QLPreviewPanel *)panel
{
    return 1;
}

- (id<QLPreviewItem>)previewPanel:(QLPreviewPanel *)panel previewItemAtIndex:(NSInteger)index
{
    Q_UNUSED(panel);
    return self.docUrl;
}

@end

void QuickLook::previewDocument(const QString &path, QWidget *parent)
{
    static DocumentPreview *preview;
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        preview = [DocumentPreview new];
    });

    if(preview.docUrl != nullptr)
        QFile::remove(QUrl::fromNSURL(preview.docUrl).toLocalFile());

    preview.docUrl = [NSURL fileURLWithPath:path.toNSString()];
    [[(__bridge NSView *)reinterpret_cast<void *>(parent->winId()) window] makeFirstResponder:preview];

    auto qlPanel = QLPreviewPanel.sharedPreviewPanel;
    if (qlPanel.isVisible)
        [qlPanel reloadData];
    else
        [qlPanel makeKeyAndOrderFront:preview];
}
