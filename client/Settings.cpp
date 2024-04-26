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

#include "Settings.h"

#include "Application.h"

#include <QtCore/QJsonValue>

template<class T, class D = T>
using Option = Settings::Option<T, D>;

const Option<bool> Settings::CDOC2_DEFAULT { QStringLiteral("CDOC2-DEFAULT"), false };
const Option<bool> Settings::CDOC2_NOTIFICATION { QStringLiteral("CDOC2-NOTIFICATION"), false };
const Option<bool> Settings::CDOC2_USE_KEYSERVER { QStringLiteral("CDOC2-USE-KEYSERVER"), true };
const Option<QString, QString (*)()> Settings::CDOC2_DEFAULT_KEYSERVER { QStringLiteral("CDOC2-DEFAULT-KEYSERVER"), [] {
	return Application::confValue(QLatin1String("CDOC2-DEFAULT-KEYSERVER")).toString(QStringLiteral("ria-test"));
}};
const Option<QString> Settings::CDOC2_GET { QStringLiteral("CDOC2-GET"), QStringLiteral(CDOC2_GET_URL) };
const Option<QByteArray> Settings::CDOC2_GET_CERT { QStringLiteral("CDOC2-GET-CERT") };
const Option<QString> Settings::CDOC2_POST { QStringLiteral("CDOC2-POST"), QStringLiteral(CDOC2_POST_URL) };
const Option<QByteArray> Settings::CDOC2_POST_CERT { QStringLiteral("CDOC2-POST-CERT") };

const Option<QString> Settings::MID_UUID { QStringLiteral("MIDUUID") };
const Option<QString> Settings::MID_NAME { QStringLiteral("MIDNAME"), QStringLiteral("RIA DigiDoc") };
const Option<QString, QString (*)()> Settings::MID_PROXY_URL { QStringLiteral("MID-PROXY-URL"), [] {
	return Application::confValue(QLatin1String("MID-PROXY-URL")).toString(QStringLiteral(MOBILEID_URL));
}};
const Option<QString, QString (*)()> Settings::MID_SK_URL { QStringLiteral("MID-SK-URL"), [] {
	return Application::confValue(QLatin1String("MID-SK-URL")).toString(QStringLiteral(MOBILEID_URL));
}};
const Option<bool, bool (*)()> Settings::MID_UUID_CUSTOM
	{ QStringLiteral("MIDUUID-CUSTOM"), [] { return Settings::MID_UUID.isSet(); } };
const Option<bool> Settings::MOBILEID_REMEMBER { QStringLiteral("MobileSettings"), true };
const Option<QString> Settings::MOBILEID_CODE { QStringLiteral("MobileCode") };
const Option<QString> Settings::MOBILEID_NUMBER { QStringLiteral("MobileNumber") };
const Option<bool> Settings::MOBILEID_ORDER { QStringLiteral("MIDOrder"), true };

const Option<QString> Settings::SID_UUID { QStringLiteral("SIDUUID") };
const Option<QString> Settings::SID_NAME { QStringLiteral("SIDNAME"), QStringLiteral("RIA DigiDoc") };
const Option<QString, QString (*)()> Settings::SID_PROXY_URL { QStringLiteral("SID-PROXY-URL"), []{
	return Application::confValue(QLatin1String("SIDV2-PROXY-URL"))
		.toString(QStringLiteral(SMARTID_URL));
}};
const Option<QString, QString (*)()> Settings::SID_SK_URL { QStringLiteral("SID-SK-URL"), []{
	return Application::confValue(QLatin1String("SIDV2-SK-URL"))
		.toString(QStringLiteral(SMARTID_URL));
}};
const Option<bool, bool (*)()> Settings::SID_UUID_CUSTOM
	{ QStringLiteral("SIDUUID-CUSTOM"), [] { return Settings::SID_UUID.isSet(); } };
const Option<bool> Settings::SMARTID_REMEMBER { QStringLiteral("SmartIDSettings"), true };
const Option<QString> Settings::SMARTID_CODE { QStringLiteral("SmartID") };
const Option<QString> Settings::SMARTID_COUNTRY { QStringLiteral("SmartIDCountry"), QStringLiteral("EE") };
const QStringList Settings::SMARTID_COUNTRY_LIST {
	QStringLiteral("EE"),
	QStringLiteral("LT"),
	QStringLiteral("LV"),
};

const Option<QByteArray, QByteArray (*)()> Settings::SIVA_CERT { QStringLiteral("SIVA-CERT"), [] {
	return Application::confValue(QLatin1String("SIVA-CERT")).toString().toLatin1();
}};
const Option<QString> Settings::SIVA_URL { QStringLiteral("SIVA-URL") };
const Option<bool, bool (*)()> Settings::SIVA_URL_CUSTOM
	{ QStringLiteral("SIVA-URL-CUSTOM"), [] { return Settings::SIVA_URL.isSet(); } };
const Option<QByteArray> Settings::TSA_CERT { QStringLiteral("TSA-CERT") };
const Option<QString> Settings::TSA_URL { QStringLiteral("TSA-URL") };
const Option<bool, bool (*)()> Settings::TSA_URL_CUSTOM
	{ QStringLiteral("TSA-URL-CUSTOM"), [] { return Settings::TSA_URL.isSet(); } };

const Option<QString> Settings::DEFAULT_DIR { QStringLiteral("DefaultDir") };
const Option<QString, QString (*)()> Settings::LANGUAGE { QStringLiteral("Language"), [] {
	auto languages = QLocale().uiLanguages();
	if(languages.first().contains(QLatin1String("et"), Qt::CaseInsensitive)) return QStringLiteral("et");
	if(languages.first().contains(QLatin1String("ru"), Qt::CaseInsensitive)) return QStringLiteral("ru");
	return QStringLiteral("en");
}};
const Option<QString> Settings::LAST_PATH { QStringLiteral("lastPath") };
Option<bool> Settings::LIBDIGIDOCPP_DEBUG { QStringLiteral("LibdigidocppDebug"), false };
const Option<bool> Settings::SETTINGS_MIGRATED { QStringLiteral("SettingsMigrated"), false };
const Option<bool> Settings::SHOW_INTRO { QStringLiteral("showIntro"), true };
const Option<bool> Settings::SHOW_PRINT_SUMMARY { QStringLiteral("ShowPrintSummary"), false };
const Option<bool> Settings::SHOW_ROLE_ADDRESS_INFO { QStringLiteral("RoleAddressInfo"), false };

const Option<int> Settings::PROXY_CONFIG { QStringLiteral("ProxyConfig"), Settings::ProxyNone };
const Option<QString> Settings::PROXY_HOST { QStringLiteral("ProxyHost") };
const Option<QString> Settings::PROXY_PORT { QStringLiteral("ProxyPort") };
const Option<QString> Settings::PROXY_USER { QStringLiteral("ProxyUser") };
const Option<QString> Settings::PROXY_PASS { QStringLiteral("ProxyPass") };
#ifdef Q_OS_MAC
const Option<QString> Settings::PLUGINS { QStringLiteral("plugins") };
const Option<bool> Settings::TSL_ONLINE_DIGEST { QStringLiteral("TSLOnlineDigest"), true };
#endif
