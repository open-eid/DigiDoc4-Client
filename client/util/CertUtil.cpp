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

#include "CertUtil.h"
#ifdef Q_OS_WIN
#include "Application.h"
#endif
#include "dialogs/CertificateDetails.h"

#include <common/SslCertificate.h>
#include <QtCore/QDir>
#include <QtCore/QFile>
#ifdef Q_OS_WIN
#include <QtCore/QUrl>
#include <QtGui/QDesktopServices>
#endif


void CertUtil::showCertificate(const SslCertificate &cert, QWidget *parent, const QString &suffix)
{
#ifdef Q_OS_LINUX
	CertificateDetails(cert, parent, true).exec();
#else
	QString name = cert.subjectInfo("serialNumber");
	if(name.isNull() || name.isEmpty())
    	name = QString("%1").arg(cert.serialNumber().constData());
	QString path = QString("%1/%2%3.cer").arg(QDir::tempPath()).arg(name).arg(suffix);
	QFile f(path);
	if(f.open(QIODevice::WriteOnly))
		f.write(cert.toPem());
	openPreview(path, parent);
#endif
}

#ifdef Q_OS_WIN
void CertUtil::openPreview(const QString &path, const QWidget *parent)
{
	qApp->addTempFile(path);
	QUrl url = QUrl::fromLocalFile(path);
	QDesktopServices::openUrl(url);
}
#endif // Q_OS_WIN
