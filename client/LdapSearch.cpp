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
#include <Windows.h>
#include <Winldap.h>
#include <Winber.h>
using STR_T = PWSTR;
#define STR(X) const_cast<STR_T>(L##X)
#define TO_STR(X) STR_T((X).utf16())
#else
#define LDAP_DEPRECATED 1
#include <sys/time.h>
#include <ldap.h>
using ULONG = int;
using LDAP_TIMEVAL = timeval;
using STR_T = char *;
#define STR(X) const_cast<STR_T>(X)
#define TO_STR(X) STR_T((X).toLocal8Bit().constData())
#endif

#include <array>
#include <chrono>

using namespace std::chrono;

template<typename T>
static constexpr auto TO_QSTR(const T *str)
{
	if constexpr (std::is_same_v<T,char>)
		return QLatin1String(str);
	else
		return QStringView(str);
}

class LdapSearch::Private
{
public:
	LDAP *ldap {};
	QUrl url;
	QTimer *timer {};
};

LdapSearch::LdapSearch(const QString &url, QObject *parent)
:	QObject( parent )
,	d(new Private)
{
	d->url = QUrl(url);
	d->timer = new QTimer(this);
	d->timer->setSingleShot(true);
	connect(d->timer, &QTimer::timeout, this, [this]{
		if(d->ldap)
			ldap_unbind_s(d->ldap);
		d->ldap = nullptr;
	});
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
	{
		d->timer->start(4min);
		return true;
	}

#ifdef Q_OS_WIN
	int ssl = d->url.scheme() == QLatin1String("ldaps") ? 1 : 0;
	QString host = d->url.host();
	ULONG port = ULONG(d->url.port(ssl ? LDAP_SSL_PORT : LDAP_PORT));
	if(d->ldap = ldap_sslinit(TO_STR(host), port, ssl); !d->ldap)
	{
		setLastError(tr("Failed to init ldap"), int(LdapGetLastError()));
		return false;
	}
	ULONG err = 0;
#else
	QByteArray host = d->url.toString(QUrl::RemovePath|QUrl::RemoveQuery|QUrl::RemoveFragment).toUtf8();
	int err = ldap_initialize(&d->ldap, host.constData());
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

	d->timer->start(4min);
	return !err;
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

	std::array<STR_T, 2> attrs { STR("userCertificate;binary"), nullptr };

	ULONG msg_id = 0;
	QString path = d->url.path();
	int err = ldap_search_ext(d->ldap, TO_STR(path.isEmpty() ? "c=EE" : path.remove(0, 1)), LDAP_SCOPE_SUBTREE,
		TO_STR(search), attrs.data(), 0, nullptr, nullptr, LDAP_NO_LIMIT, LDAP_NO_LIMIT, &msg_id);
	if(err)
		return setLastError( tr("Failed to init ldap search"), err );

	auto *timer = new QTimer(this);
	connect(timer, &QTimer::timeout, this, [this, msg_id, timer, userData] {
		LDAPMessage *result = nullptr;
		LDAP_TIMEVAL t { 5, 0 };
		switch(int err = ldap_result(d->ldap, msg_id, LDAP_MSG_ALL, &t, &result))
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
			for(auto *attr = ldap_first_attribute(d->ldap, entry, &pos);
				attr; attr = ldap_next_attribute(d->ldap, entry, pos))
			{
				if(QLatin1String("userCertificate;binary") == TO_QSTR(attr))
				{
					berval **cert = ldap_get_values_len(d->ldap, entry, attr);
					for(ULONG i = 0; i < ldap_count_values_len(cert); ++i)
						list.append(QSslCertificate(QByteArray::fromRawData(cert[i]->bv_val, int(cert[i]->bv_len)), QSsl::Der));
					ldap_value_free_len(cert);
				}
				ldap_memfree(attr);
			}
			ber_free(pos, 0);
		}
		ldap_msgfree(result);

		Q_EMIT searchResult(list, count, userData);
	});
	timer->start(1s);
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

