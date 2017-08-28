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

#include <QtCore/QCoreApplication>
#include <QtCore/QFile>
#include <QtCore/QUrl>
#include <QtCore/QXmlStreamReader>
#include <QtCore/QXmlStreamWriter>
#include <QtNetwork/QNetworkRequest>
#include <QtNetwork/QNetworkReply>
#include <QtNetwork/QNetworkAccessManager>

int main(int argc, char *argv[])
{
	QCoreApplication a(argc, argv);
	QStringList territories = a.arguments();
	territories.removeFirst();
	QString path = territories.first();
	territories.removeFirst();

	QNetworkAccessManager m;
	QObject::connect(&m, &QNetworkAccessManager::sslErrors, [](QNetworkReply *r, const QList<QSslError> &errors){
		r->ignoreSslErrors(errors);
	});

	QNetworkReply *r = m.get(QNetworkRequest(QUrl(
		"https://ec.europa.eu/information_society/policy/esignature/trusted-list/tl-mp.xml")));
	QObject::connect(r, &QNetworkReply::finished, [&](){
		QFile f(path + "/" + r->request().url().fileName());
		if(f.open(QFile::ReadWrite))
			f.write(r->readAll());

		f.seek(0);
		QXmlStreamReader xml( &f );
		QString url, territory;
		while(xml.readNext() != QXmlStreamReader::Invalid)
		{
			if(!xml.isStartElement())
				continue;
			if(xml.name() == "TSLLocation")
				url = xml.readElementText();
			else if( xml.name() == "SchemeTerritory")
				territory = xml.readElementText();
			else if(xml.name() == "MimeType" && xml.readElementText() == "application/vnd.etsi.tsl+xml" && territories.contains(territory))
			{
				QNetworkReply *rt = m.get(QNetworkRequest(QUrl(url)));
				QEventLoop e;
				QObject::connect(rt, &QNetworkReply::finished, [&](){
					QFile t(path + "/" + territory + ".xml");
					if(t.open(QFile::WriteOnly))
						t.write(rt->readAll());
					e.quit();
				});
				e.exec();
				url.clear();
				territory.clear();
			}
		}

		QFile o(path + "/TSL.qrc");
		o.open(QFile::WriteOnly);
		QXmlStreamWriter w(&o);
		w.writeStartElement("RCC");
		w.writeStartElement("qresource");
		w.writeAttribute("prefix", "TSL");
		w.writeTextElement("file", r->request().url().fileName());
		for(const QString &territory: territories)
			w.writeTextElement("file", territory + ".xml");
		w.writeEndElement();
		w.writeEndElement();

		a.quit();
	});
	return a.exec();
}
