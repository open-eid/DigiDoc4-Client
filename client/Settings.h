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

struct Settings
{
	template<class T, class D = T>
	struct Option
	{
		operator QVariant() const {
			if(QSettings s(isLocked() ? QSettings::SystemScope : QSettings::UserScope); s.contains(KEY))
				return s.value(KEY);
			return defaultValue();
		}
		operator T() const {
			return operator QVariant().template value<T>();
		}
		operator std::string() const requires(std::is_same_v<T,QString>) {
			return operator T().toStdString();
		}
		void operator =(const QVariant &value) const {
			if(value == defaultValue())
				clear();
			else
				QSettings().setValue(KEY, value);
			if(f)
				f(operator T());
		}
		void operator() (const T &value) const {
			operator =(QVariant(value));
		}
		void clear() const {
			QSettings().remove(KEY);
		}
		bool isLocked() const {
			return QSettings(QSettings::SystemScope).value(KEY + QLatin1String("_LOCK"), false).toBool();
		}
		bool isSet() const {
			return QSettings().contains(KEY);
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
		const QString KEY;
		const D DEFAULT {};
		std::function<void (const T &value)> f {};
	};

	static const Option<bool, bool (*)()> CDOC2_DEFAULT;
	static const Option<bool> CDOC2_NOTIFICATION;
	static const Option<bool, bool (*)()> CDOC2_USE_KEYSERVER;
	static const Option<QString, QString (*)()> CDOC2_DEFAULT_KEYSERVER;
	static const Option<QString> CDOC2_UUID;
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
	static const Option<bool> SHOW_INTRO;
	static const Option<bool> SHOW_PRINT_SUMMARY;
	static const Option<bool> SHOW_ROLE_ADDRESS_INFO;

	enum ProxyConfig: quint8 {
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
#endif
};
