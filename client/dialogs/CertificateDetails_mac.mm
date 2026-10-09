// SPDX-FileCopyrightText: Estonian Information System Authority
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "CertificateDetails.h"

#include "SslCertificate.h"

#include <QtCore/QDir>
#include <QtCore/QFile>
#include <QtCore/QUrl>

#import <QuickLookUI/QuickLookUI.h>

@interface CertPreview : NSResponder <QLPreviewPanelDataSource>
@property (nonatomic, copy) NSURL *certUrl;
@end

@implementation CertPreview

- (NSInteger)numberOfPreviewItemsInPreviewPanel:(QLPreviewPanel *)panel
{
	return 1;
}

- (id<QLPreviewItem>)previewPanel:(QLPreviewPanel *)panel previewItemAtIndex:(NSInteger)index
{
	Q_UNUSED(panel)
	return self.certUrl;
}

@end


void CertificateDetails::showCertificate(const QSslCertificate &cert, QWidget *parent, const QString &suffix)
{
	QString name = cert.subjectInfo("serialNumber").join('_');
	if(name.isNull() || name.isEmpty())
		name = QString::fromUtf8(cert.serialNumber());
	QString path = QStringLiteral("%1/%2%3.cer").arg(QDir::tempPath(), name, suffix);

	static CertPreview *cp;
	static dispatch_once_t onceToken;
	dispatch_once(&onceToken, ^{
		cp = [CertPreview new];
	});

	if(cp.certUrl != nullptr)
		QFile::remove(QUrl::fromNSURL(cp.certUrl).toLocalFile());
	if(QFile f(path); f.open(QIODevice::WriteOnly))
		f.write(cert.toPem());
	cp.certUrl = [NSURL fileURLWithPath:path.toNSString()];

	auto qlPanel = QLPreviewPanel.sharedPreviewPanel;
	if (qlPanel.isVisible)
		[qlPanel reloadData];
	else
	{
		qlPanel.dataSource = cp;
		[qlPanel makeKeyAndOrderFront:nil];
	}
}
