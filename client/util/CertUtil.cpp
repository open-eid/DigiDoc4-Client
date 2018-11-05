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
#include "dialogs/CertificateDetails.h"

#include <common/SslCertificate.h>
#ifdef Q_OS_WIN
#include <qt_windows.h>
#include <cryptuiapi.h>
#endif

#ifndef Q_OS_MAC
void CertUtil::showCertificate(const SslCertificate &cert, QWidget *parent, const QString &suffix)
{
#ifdef Q_OS_LINUX
	CertificateDetails(cert, parent, true).exec();
#else
	CRYPTUI_VIEWCERTIFICATE_STRUCT params = {};
	params.dwSize = sizeof(CRYPTUI_VIEWCERTIFICATE_STRUCT);
	params.hwndParent = HWND(parent->window()->winId());
	params.dwFlags = CRYPTUI_HIDE_HIERARCHYPAGE|CRYPTUI_DISABLE_EDITPROPERTIES|CRYPTUI_DISABLE_ADDTOSTORE;
	QByteArray der = cert.toDer();
	params.pCertContext = CertCreateCertificateContext(X509_ASN_ENCODING, PBYTE(der.constData()), DWORD(der.size()));
	BOOL propertiesChanged = FALSE;
	CryptUIDlgViewCertificate(&params, &propertiesChanged);
	CertFreeCertificateContext(params.pCertContext);
#endif
}
#endif
