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

#include <QtCore/QCoreApplication>
#include <QtCore/QFile>
#include <QtCore/QUrl>
#include <QtCore/QXmlStreamReader>
#include <QtNetwork/QNetworkRequest>
#include <QtNetwork/QNetworkReply>
#include <QtNetwork/QNetworkAccessManager>

int main(int argc, char *argv[])
{
	QCoreApplication a(argc, argv);
	QStringList territories = QCoreApplication::arguments();
	territories.removeFirst();
	QString path = territories.takeFirst();
	QString url = territories.takeFirst();

	QNetworkAccessManager m;
	QObject::connect(&m, &QNetworkAccessManager::sslErrors, &m, [](QNetworkReply *r, const QList<QSslError> &errors){
		r->ignoreSslErrors(errors);
	});

	QNetworkReply *r = m.get(QNetworkRequest(QUrl(url)));
	QObject::connect(r, &QNetworkReply::finished, r, [&] {
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
			if(xml.name() == QLatin1String("TSLLocation"))
				url = xml.readElementText();
			else if( xml.name() ==  QLatin1String("SchemeTerritory"))
				territory = xml.readElementText();
			else if(xml.name() ==  QLatin1String("MimeType") &&
					 xml.readElementText() == QLatin1String("application/vnd.etsi.tsl+xml") &&
				territories.contains(territory))
			{
				QNetworkReply *rt = m.get(QNetworkRequest(QUrl(url)));
				QEventLoop e;
				QObject::connect(rt, &QNetworkReply::finished, rt, [&](){
					QFile t(QStringLiteral("%1/%2.xml").arg(path, territory));
					if(t.open(QFile::WriteOnly))
						t.write(rt->readAll());
					e.quit();
				});
				e.exec();
				url.clear();
				territory.clear();
			}
		}

		QCoreApplication::quit();
	});
	return QCoreApplication::exec();
}
