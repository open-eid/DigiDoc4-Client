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

#include "Styles.h"
#include "Settings.h"

#include <QDebug>
#include <QFontDatabase>
#include <QFontMetrics>
#include <QGuiApplication>
#include <QImage>
#include <QPixmap>
#include <QScreen>
#include <QVariantList>

#ifndef Q_OS_MAC
	// https://forum.qt.io/topic/26663/different-os-s-different-font-sizes/3
	#define FONT_SIZE_DECREASE_SMALL 3 
	#define FONT_SIZE_DECREASE_LARGE 4
	#define FONT_DECREASE_CUTOFF 12
#endif

class FontDatabase
{
public:
	FontDatabase()
	{
		bold = QFontDatabase::applicationFontFamilies(
			QFontDatabase::addApplicationFont(":/fonts/Roboto-Bold.ttf")
		).at(0);
		condensed = QFontDatabase::applicationFontFamilies(
			QFontDatabase::addApplicationFont(":/fonts/RobotoCondensed-Regular.ttf")
		).at(0);
		condensedBold = QFontDatabase::applicationFontFamilies(
			QFontDatabase::addApplicationFont(":/fonts/RobotoCondensed-Bold.ttf")
		).at(0);
		regular = QFontDatabase::applicationFontFamilies(
			QFontDatabase::addApplicationFont(":/fonts/Roboto-Regular.ttf")
		).at(0);

	//	Width should be 470
		QString text = "Allkirjastamiseks või kontrollimiseks lohista fail siia…";
		qDebug() << "Width of text '" << text << "' with font size 20 should be 470";
		for(auto size = 12; size <= 20; size++)
		{
			qDebug() << "Font size " << size 
				<< ": width " << QFontMetrics(QFont(regular, size)).width(text)
				<<  "/ bold " << QFontMetrics(QFont(bold, size)).width(text);
		}
		text = "MUUDA";
		qDebug() << "Width of text '" << text << "' with condensed font size 12 should be 36";
		for(auto size = 6; size <= 12; size++)
		{
			qDebug() << "Font size " << size 
				<< ": width " << QFontMetrics(QFont(condensed, size)).width(text)
				<<  "/ bold " << QFontMetrics(QFont(condensedBold, size)).width(text);
		}
		// http://doc.qt.io/qt-5/highdpi.html
		// http://doc.qt.io/qt-5/scalability.html
		qreal refDpi = 93.;
		qreal refHeight = 1920.;
		qreal refWidth = 1200.;
		QRect rect = QGuiApplication::primaryScreen()->geometry();
		qreal height = qMax(rect.width(), rect.height());
		qreal width = qMin(rect.width(), rect.height());
		qreal dpi = QGuiApplication::primaryScreen()->logicalDotsPerInch();
		qreal ratio = qMin(height/refHeight, width/refWidth);
		qreal ratioFont = qMin(height*refDpi/(dpi*refHeight), width*refDpi/(dpi*refWidth));
		qDebug() << "DPI: " << dpi << " Calc ratio:" << ratio << " Calc font ratio: " << ratioFont;
	};
	QString fontName( Styles::Font font )
	{
		switch( font )
		{
			case Styles::Bold: return bold;
			case Styles::Condensed: return condensed;
			case Styles::CondensedBold: return condensedBold;
			default: return regular;
		}
	}
	QFont font( Styles::Font font, int size )
	{
#ifdef Q_OS_MAC
		return QFont(fontName(font), size);
#else
		int decrease = size > FONT_DECREASE_CUTOFF ? FONT_SIZE_DECREASE_LARGE : FONT_SIZE_DECREASE_SMALL;
		return QFont( fontName( font ), size - decrease );
#endif
	};

private:
	QString bold;
	QString condensed;
	QString condensedBold;
	QString regular;
};

QFont Styles::font( Styles::Font font, int size )
{
	static FontDatabase fontDatabase;
	return fontDatabase.font( font, size );
}

QFont Styles::font( Styles::Font font, int size, QFont::Weight weight )
{
	QFont f = Styles::font( font, size );
	f.setWeight( weight );
	return f;
}

void Styles::cachedPicture( const QString &id, std::vector<PictureInterface*> pictureWidgets )
{
	Settings settings;
	QVariantList index = settings.value("imageIndex").toList();

	if( index.contains(id) )
	{
		QImage image = settings.value("imageCache").toMap().value( id ).value<QImage>();
		QPixmap pixmap = QPixmap::fromImage( image );
		for( auto widget: pictureWidgets )
		{
			widget->showPicture( pixmap );
		}
	}
}
