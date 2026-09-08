// SPDX-FileCopyrightText: Estonian Information System Authority
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include <QtCore/QObject>
#include <QtNetwork/QSslConfiguration>

class QSslCertificate;
class QUrl;
class LdapSearch final: public QObject
{
	Q_OBJECT

public:
	enum Match : quint8 {
		Equals,
		Contains,
	};

	explicit LdapSearch(QObject *parent = nullptr);

	void search(const QUrl &url, const QString &attribute, const QString &value, Match match, const QVariantMap &userData);

Q_SIGNALS:
	void searchResult(const QList<QSslCertificate> &result, int resultCount, const QVariantMap &userData);
	void error(const QString &title, const QString &details);

private:
	QSslConfiguration ssl = QSslConfiguration::defaultConfiguration();
};
