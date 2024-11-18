/*
 * QDigiDocClient
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

#include <QtNetwork/QNetworkReply>

class CheckConnection
{
public:
	bool check(const QUrl &url = QStringLiteral("https://id.eesti.ee/config.json"));
	QString errorString() const;
	QString errorDetails() const;

	static QNetworkAccessManager* setupNAM(QNetworkRequest &req, const QByteArray &add = {});
	static QNetworkAccessManager* setupNAM(QNetworkRequest &req,
		const QSslCertificate &cert, const QSslKey &key, const QByteArray &add = {});
	static QSslConfiguration sslConfiguration(const QByteArray &add = {});

private:
	QNetworkReply::NetworkError m_error = QNetworkReply::NoError;
	QString qtmessage;
};
