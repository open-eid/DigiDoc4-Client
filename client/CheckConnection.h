// SPDX-FileCopyrightText: Estonian Information System Authority
// SPDX-License-Identifier: LGPL-2.1-or-later

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
