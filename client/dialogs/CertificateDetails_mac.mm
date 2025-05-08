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
