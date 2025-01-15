/*
 * QEstEidCommon
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
#include "Diagnostics.h"

#include "Application.h"
#include "Common.h"
#include "QPCSC.h"
#include "Settings.h"

#include <QtCore/QDir>
#include <QtCore/QJsonObject>
#include <QtCore/QTextStream>
#include <QtNetwork/QSslCertificate>

void Diagnostics::generalInfo(QTextStream &s)
{
	s << "<b>" << tr("Arguments:") << "</b> " << Application::arguments().join(' ') << "<br />"
		<< "<b>" << tr("Library paths:") << "</b> " << QCoreApplication::libraryPaths().join(';') << "<br />"
		<< "<b>" << "URLs:" << "</b>"
#ifdef CONFIG_URL
		<< "<br />CONFIG_URL: " << CONFIG_URL
#endif
		<< "<br />SIDV2-PROXY-URL: " << Settings::SID_PROXY_URL
		<< "<br />SIDV2-SK-URL: " << Settings::SID_SK_URL
		<< "<br />MID-PROXY-URL: " << Settings::MID_PROXY_URL
		<< "<br />MID-SK-URL: " << Settings::MID_SK_URL
		<< "<br />RPUUID: " << (Settings::MID_UUID_CUSTOM ? tr("is set manually") : tr("is set by default"))
		<< "<br />TSL_URL: " << Application::confValue(Application::TSLUrl).toString()
		<< "<br />TSA_URL: " << Application::confValue(Application::TSAUrl).toString()
		<< "<br />SIVA_URL: " << Application::confValue(Application::SiVaUrl).toString()
		<< "<br /><b>CDOC2:</b>"
		<< "<br />" << Settings::CDOC2_DEFAULT.KEY << ": " << (Settings::CDOC2_DEFAULT ? tr("true") : tr("false"))
		<< "<br />" << Settings::CDOC2_USE_KEYSERVER.KEY << ": " << (Settings::CDOC2_USE_KEYSERVER ? tr("true") : tr("false"))
		<< "<br />" << Settings::CDOC2_DEFAULT_KEYSERVER.KEY << ": " << Settings::CDOC2_DEFAULT_KEYSERVER
		<< "<br /><br /><b>" << tr("TSL signing certs") << ":</b>";
	for(const QSslCertificate &cert: Application::confValue(Application::TSLCerts).value<QList<QSslCertificate>>())
		s << "<br />" << cert.subjectInfo("CN").value(0);
	s << "<br /><br /><b>" << tr("TSL cache") << ":</b>";
	QString cache = Application::confValue(Application::TSLCache).toString();
	const QStringList tsllist = QDir(cache).entryList({QStringLiteral("*.xml")});
	for(const QString &file: tsllist)
	{
		if(uint ver = Application::readTSLVersion(cache + "/" + file); ver > 0)
			s << "<br />" << file << " (" << ver << ")";
	}
	s << "<br /><br />";

#ifdef CONFIG_URL
	s << "<b>" << tr("Central Configuration") << ":</b>";
	QJsonObject metainf = Application::confValue(QLatin1String("META-INF")).toObject();
	for(QJsonObject::const_iterator i = metainf.constBegin(), end = metainf.constEnd(); i != end; ++i)
	{
		if(i.value().type() == QJsonValue::Double)
			s << "<br />" << i.key() << ": " << i.value().toInt();
		else
			s << "<br />" << i.key() << ": " << i.value().toString();
	}
	s << "<br /><br />";
#endif

	s << "<b>" << tr("Smart Card service status: ") << "</b>" << " "
		<< (QPCSC::instance().serviceRunning() ? tr("Running") : tr("Not running"));

	s << "<br /><b>" << tr("Smart Card readers") << ":</b><br />";
	for( const QString &readername: QPCSC::instance().readers() )
	{
		s << readername;
		QPCSCReader reader( readername, &QPCSC::instance() );
		if( !reader.isPresent() )
		{
#ifndef Q_OS_WIN /* Apple 10.5.7 and pcsc-lite previous to v1.5.5 do not support 0 as protocol identifier */
			reader.connect( QPCSCReader::Direct );
#else
			reader.connect( QPCSCReader::Direct, QPCSCReader::Undefined );
#endif
		}
		else
			reader.connect();

		if(readername.contains(QStringLiteral("EZIO SHIELD"), Qt::CaseInsensitive))
		{
			s << " - Secure PinPad";
			if( !reader.isPinPad() )
				s << " (Driver missing)";
		}
		else if( reader.isPinPad() )
			s << " - PinPad";

		QHash<QPCSCReader::Properties,int> prop = reader.properties();
		if(prop.contains(QPCSCReader::dwMaxAPDUDataSize))
			s << " max APDU size " << prop.value(QPCSCReader::dwMaxAPDUDataSize);
		s << "<br />" << "Reader state: " << reader.state().join(QStringLiteral(", ")) << "<br />";
		if( !reader.isPresent() )
			continue;

		reader.reconnect( QPCSCReader::UnpowerCard );
		QString cold = reader.atr();
		reader.reconnect( QPCSCReader::ResetCard );
		QString warm = reader.atr();

		s << "ATR cold - " << cold << "<br />"
		  << "ATR warm - " << warm << "<br />";

		reader.beginTransaction();
		constexpr auto APDU = &QByteArray::fromHex;
		auto printAID = [&](const QString &label, const QByteArray &apdu)
		{
			constexpr auto APDU = &QByteArray::fromHex;
			QPCSCReader::Result r = reader.transfer(apdu);
			s << label << ": " << Qt::hex << r.SW;
			if (r.SW == 0x9000) s << " (OK)";
			if (r.SW == 0x6A81) s << " (Locked)";
			if (r.SW == 0x6A82) s << " (Not found)";
			s << "<br />";
			return r;
		};
		if(printAID(QStringLiteral("AID_IDEMIA"), APDU("00A40400 10 A000000077010800070000FE00000100")) ||
			printAID(QStringLiteral("AID_OT"), APDU("00A4040C 0D E828BD080FF2504F5420415750")) ||
			printAID(QStringLiteral("AID_QSCD"), APDU("00A4040C 10 51534344204170706C69636174696F6E")))
		{
			reader.transfer(APDU("00A4000C"));
			reader.transfer(APDU("00A4010C025000"));
			reader.transfer(APDU("00A4010C025006"));
			s << "ID - " << reader.transfer(APDU("00B00000 00")).data << "<br />";
		}
		reader.endTransaction();
	}

#ifdef Q_OS_WIN
	s << "<b>" << tr("Smart Card reader drivers") << ":</b><br />" << Common::drivers().join(QLatin1String("<br />"));
#endif
}
