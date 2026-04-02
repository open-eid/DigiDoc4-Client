// SPDX-FileCopyrightText: Estonian Information System Authority
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include <QtCore/QObject>

class QSslCertificate;
class LdapSearch final: public QObject
{
	Q_OBJECT

public:
	LdapSearch(const QString &url, QObject *parent = nullptr);
	~LdapSearch() final;

	void search(const QString &search, const QVariantMap &userData);

Q_SIGNALS:
	void searchResult(const QList<QSslCertificate> &result, int resultCount, const QVariantMap &userData);
	void error(const QString &title, const QString &details);

private:
	bool init();
	void setLastError( const QString &msg, int err );

	class Private;
	Private *d;
};
