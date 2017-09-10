/*
 * QEstEidUtil
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

#pragma once

#include <QtCore/QObject>

class QSslCertificate;

class SSLConnectPrivate;
class SSLConnect: public QObject
{
	Q_OBJECT

public:
	enum RequestType {
		EmailInfo,
		ActivateEmails,
		MobileInfo,
		PictureInfo
	};

	explicit SSLConnect( QObject *parent = 0 );
	~SSLConnect();

	QString errorString() const;
	QByteArray getUrl( RequestType type, const QString &value = QString() );
	void setToken( const QSslCertificate &cert, Qt::HANDLE key );

private:
	SSLConnectPrivate	*d;
};
