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

#pragma once

#include <QtCore/QCoreApplication>
#include <QtCore/QSettings>

template<typename T>
using if_QString = std::enable_if_t<std::is_same_v<T,QString>, bool>;

struct Settings
{
	template<class T, class D = T>
	struct Option
	{
		operator QVariant() const {
			return settings().value(KEY, defaultValue());
		}
		operator T() const {
			return operator QVariant().template value<T>();
		}
		template <typename P = T, typename = if_QString<P>>
		operator std::string() const {
			return operator T().toStdString();
		}
		void operator =(const QVariant &value) const {
			setValue(value, defaultValue());
		}
		void operator =(const T &value) const {
			operator =(QVariant(value));
		}
		template <typename P = T, typename = if_QString<P>>
		void operator =(const std::string &value) const {
			operator =(QString::fromStdString(value));
		}
		void clear() const {
			QSettings().remove(KEY);
		}
		bool isLocked() const {
			return settings(QSettings::SystemScope).value(KEY + QLatin1String("_LOCK"), false).toBool();
		}
		bool isSet() const {
			return QSettings().contains(KEY);
		}
		T value(const QVariant &def) const {
			return settings().value(KEY, def).template value<T>();
		}
		void setValue(const QVariant &value, const QVariant &def = {}) const {
			if(bool valueIsNullOrEmpty = value.type() == QVariant::String ? value.toString().isEmpty() : value.isNull();
				value == def || (def.isNull() && valueIsNullOrEmpty))
				clear();
			else
				QSettings().setValue(KEY, value);
			if(f)
				f(operator T());
		}
		T defaultValue() const {
			if constexpr (std::is_invocable_v<D>)
				return DEFAULT();
			else
				return DEFAULT;
		}
		template <typename F>
		void registerCallback(F functor)
		{
			f = functor;
		}
		QSettings settings() const {
			return settings(isLocked() ? QSettings::SystemScope : QSettings::UserScope);
		}
		static QSettings settings(QSettings::Scope scope) {
#if QT_VERSION >= QT_VERSION_CHECK(5, 13, 0)
			return QSettings(scope);
#else
			return QSettings(scope, QCoreApplication::organizationName(), QCoreApplication::applicationName());
#endif
		}
		const QString KEY;
		const D DEFAULT {};
		std::function<void (const T &value)> f {};
	};

	static const Option<bool> CDOC2_DEFAULT;
	static const Option<bool> CDOC2_NOTIFICATION;
	static const Option<bool> CDOC2_USE_KEYSERVER;
	static const Option<QString, QString (*)()> CDOC2_DEFAULT_KEYSERVER;
	static const Option<QString> CDOC2_GET;
	static const Option<QByteArray> CDOC2_GET_CERT;
	static const Option<QString> CDOC2_POST;
	static const Option<QByteArray> CDOC2_POST_CERT;

	static const Option<QString> MID_UUID;
	static const Option<QString> MID_NAME;
	static const Option<QString, QString (*)()> MID_PROXY_URL;
	static const Option<QString, QString (*)()> MID_SK_URL;
	static const Option<bool, bool (*)()> MID_UUID_CUSTOM;
	static const Option<bool> MOBILEID_REMEMBER;
	static const Option<QString> MOBILEID_CODE;
	static const Option<QString> MOBILEID_NUMBER;
	static const Option<bool> MOBILEID_ORDER;

	static const Option<QString> SID_UUID;
	static const Option<QString> SID_NAME;
	static const Option<QString, QString (*)()> SID_PROXY_URL;
	static const Option<QString, QString (*)()> SID_SK_URL;
	static const Option<bool, bool (*)()> SID_UUID_CUSTOM;
	static const Option<bool> SMARTID_REMEMBER;
	static const Option<QString> SMARTID_CODE;
	static const Option<QString> SMARTID_COUNTRY;
	static const QStringList SMARTID_COUNTRY_LIST;

	static const Option<QByteArray, QByteArray (*)()> SIVA_CERT;
	static const Option<QString> SIVA_URL;
	static const Option<bool, bool (*)()> SIVA_URL_CUSTOM;
	static const Option<QByteArray> TSA_CERT;
	static const Option<QString> TSA_URL;
	static const Option<bool, bool (*)()> TSA_URL_CUSTOM;

	static const Option<QString> DEFAULT_DIR;
	static const Option<QString, QString (*)()> LANGUAGE;
	static const Option<QString> LAST_PATH;
	static Option<bool> LIBDIGIDOCPP_DEBUG;
	static const Option<bool> SETTINGS_MIGRATED;
	static const Option<bool> SHOW_INTRO;
	static const Option<bool> SHOW_PRINT_SUMMARY;
	static const Option<bool> SHOW_ROLE_ADDRESS_INFO;

	enum ProxyConfig {
		ProxyNone,
		ProxySystem,
		ProxyManual,
	};
	static const Option<int> PROXY_CONFIG;
	static const Option<QString> PROXY_HOST;
	static const Option<QString> PROXY_PORT;
	static const Option<QString> PROXY_USER;
	static const Option<QString> PROXY_PASS;
#ifdef Q_OS_MAC
	static const Option<QString> PLUGINS;
	static const Option<bool> TSL_ONLINE_DIGEST;
#endif
};
