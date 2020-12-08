/*
 * QDigiDocCrypto
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

#include "LdapSearch.h"

#include <QtCore/QTimer>
#include <QtCore/QUrl>
#include <QtCore/QVariantMap>
#include <QtNetwork/QSslCertificate>

#ifdef Q_OS_WIN
#undef UNICODE
#include <Windows.h>
#include <Winldap.h>
#include <Winber.h>
#else
#define LDAP_DEPRECATED 1
#include <sys/time.h>
#include <ldap.h>
using ULONG = int;
using LDAP_TIMEVAL = timeval;
#endif


class LdapSearch::Private
{
public:
	LDAP *ldap = nullptr;
	QByteArray host;
};

LdapSearch::LdapSearch(QByteArray host, QObject *parent)
:	QObject( parent )
,	d(new Private)
{
	d->host = std::move(host);
}

LdapSearch::~LdapSearch()
{
	if( d->ldap )
		ldap_unbind_s( d->ldap );
	delete d;
}

bool LdapSearch::init()
{
	if(d->ldap)
		return true;

#ifdef Q_OS_WIN
	QUrl url(d->host);
	int ssl = url.scheme() == QStringLiteral("ldaps") ? 1 : 0;
	QString host = url.host();
	ULONG port = ULONG(url.port(ssl ? LDAP_SSL_PORT : LDAP_PORT));
	if(!(d->ldap = ldap_sslinit(const_cast<char*>(host.toLocal8Bit().constData()), port, ssl)))
	{
		setLastError(tr("Failed to init ldap"), int(LdapGetLastError()));
		return false;
	}
	ULONG err = 0;
#else
	int err = ldap_initialize(&d->ldap, d->host.constData());
	if(err)
	{
		setLastError(tr("Failed to init ldap"), err);
		return false;
	}
#endif

	int version = LDAP_VERSION3;
	err = ldap_set_option(d->ldap, LDAP_OPT_PROTOCOL_VERSION, &version);
	if(err)
	{
		setLastError(tr("Failed to set ldap version"), err);
		return false;
	}

#ifndef Q_OS_WIN
#if 1
	int cert_flag = LDAP_OPT_X_TLS_NEVER;
	err = ldap_set_option(nullptr, LDAP_OPT_X_TLS_REQUIRE_CERT, &cert_flag);
	if(err)
	{
		setLastError(tr("Failed to start ssl"), err);
		return false;
	}
#else
	err = ldap_set_option(nullptr, LDAP_OPT_X_TLS_CACERTFILE, "");
	if(err)
	{
		setLastError(tr("Failed to start ssl"), err);
		return false;
	}
#endif
#endif

	err = ldap_simple_bind_s(d->ldap, nullptr, nullptr);
	if(err)
		setLastError(tr("Failed to init ldap"), err);

	QTimer::singleShot(4*60*60, this, [this]{
		if(d->ldap)
			ldap_unbind_s(d->ldap);
		d->ldap = nullptr;
	});
	return !err;
}

bool LdapSearch::isSSL() const
{
	return QUrl(d->host).scheme() == QStringLiteral("ldaps");
}

void LdapSearch::search(const QString &search, const QVariantMap &userData)
{
	if(!init())
	{
		if(d->ldap)
			ldap_unbind_s(d->ldap);
		d->ldap = nullptr;
		return;
	}

	char *attrs[] = { const_cast<char*>("userCertificate;binary"), nullptr };

	ULONG msg_id = 0;
	int err = ldap_search_ext( d->ldap, "c=EE", LDAP_SCOPE_SUBTREE,
		const_cast<char*>(search.toLocal8Bit().constData()), attrs, 0, nullptr, nullptr, LDAP_NO_LIMIT, LDAP_NO_LIMIT, &msg_id);
	if(err)
		return setLastError( tr("Failed to init ldap search"), err );

	QTimer *timer = new QTimer(this);
	connect(timer, &QTimer::timeout, this, [this, msg_id, timer, userData] {
		LDAPMessage *result = nullptr;
		LDAP_TIMEVAL t = { 5, 0 };
		int err = ldap_result(d->ldap, msg_id, LDAP_MSG_ALL, &t, &result);
		switch(err)
		{
		case LDAP_SUCCESS: //Timeout
			return;
		case LDAP_RES_SEARCH_ENTRY:
		case LDAP_RES_SEARCH_RESULT:
			break;
		default:
			setLastError(tr("Failed to get result"), err);
			timer->deleteLater();
			return;
		}
		timer->deleteLater();

		QList<QSslCertificate> list;
		LDAPMessage *entry = ldap_first_entry(d->ldap, result);
		int count = ldap_count_entries(d->ldap, entry);
		for(; entry; entry = ldap_next_entry(d->ldap, entry))
		{
			BerElement *pos = nullptr;
			for(char *attr = ldap_first_attribute(d->ldap, entry, &pos);
				attr; attr = ldap_next_attribute(d->ldap, entry, pos))
			{
				if( qstrcmp( attr, "userCertificate;binary" ) == 0 )
				{
					berval **cert = ldap_get_values_len(d->ldap, entry, attr);
					for(ULONG i = 0; i < ldap_count_values_len(cert); ++i)
						list << QSslCertificate(QByteArray::fromRawData(cert[i]->bv_val, int(cert[i]->bv_len)), QSsl::Der);
					ldap_value_free_len(cert);
				}
				ldap_memfree(attr);
			}
			ber_free(pos, 0);
		}
		ldap_msgfree(result);

		Q_EMIT searchResult(list, count, userData);
	});
	timer->start(1000);
}

void LdapSearch::setLastError( const QString &msg, int err )
{
	QString res = msg;
	QString details;
	switch( err )
	{
	case LDAP_UNAVAILABLE:
	case LDAP_SERVER_DOWN:
		res = tr("Check your internet connection!\nLDAP server is unavailable.");
		break;
	default:
		details = tr( "Error Code: %1 (%2)" ).arg( err ).arg( ldap_err2string( err ) );
		break;
	}
	Q_EMIT error( res, details );
}

