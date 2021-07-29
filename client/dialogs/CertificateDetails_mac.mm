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

#if defined(Q_OS_MAC)
#include "QuickLook_mac.h"
#endif

#include <QtCore/QDir>
#include <QtCore/QFile>
#include <QtCore/QUrl>
#include <QtWidgets/QWidget>
#include <QtCore/QDebug>

#import <objc/runtime.h>
#import <Quartz/Quartz.h>

void CertificateDetails::showCertificate(const SslCertificate &cert, QWidget *parent, const QString &suffix)
{
	QString name = cert.subjectInfo("serialNumber");
	if(name.isNull() || name.isEmpty())
		name = QString::fromUtf8(cert.serialNumber());
	QString path = QStringLiteral("%1/%2%3.cer").arg(QDir::tempPath(), name, suffix);
	QFile f(path);
	if(f.open(QIODevice::WriteOnly))
		f.write(cert.toPem());
	f.close();

	QuickLook::previewDocument(path, parent);

}


