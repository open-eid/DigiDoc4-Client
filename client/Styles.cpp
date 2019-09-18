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

#include <QFontDatabase>
#ifndef Q_OS_MAC
#include <QFontMetrics>
#endif
#include <QImage>
#ifndef Q_OS_MAC
#include <QMap>
#endif
#include <QPixmap>
#include <QVariantList>

#ifndef Q_OS_MAC
// https://forum.qt.io/topic/26663/different-os-s-different-font-sizes/3
// http://doc.qt.io/qt-5/scalability.html
#define FONT_SIZE_DECREASE_SMALL 3 
#define FONT_SIZE_DECREASE_LARGE 4
#define FONT_DECREASE_CUTOFF 12
#define MIN_FONT_SIZE 2

struct FontSample {
	int fontSize;
	const char *sample;
	int width;
};

const FontSample condensedSamples[] = {
	{ 9, "LAADI", 30 }, // Estimated
	{ 11, "DOKUMENT", 51 },
	{ 12, "MUUDA", 36 },
	{ 14, "KATKESTA", 59 },
	{ 16, "Lugejas on ID kaart", 120 },
	{ 20, "MARI MAASIKAS MUSTIKAS", 227 },
	{ 24, "MINU eID", 88 },
};

const FontSample regularSamples[] = {
	{ 11, "Dokumendi haldamiseks sisesta kaart lugejasse", 233 },
	{ 12, "Ver. 4.0.0.1", 57 },
	{ 13, "Dokument2.pdf", 80 }, // Estimated
	{ 14, "Dokument2.pdf", 96 },
	{ 15, "MARI MAASIKAS", 110 }, // Estimated
	{ 16, "MARI MAASIKAS", 124 },
	{ 18, "Isikutuvastamise sertifikaat", 221 },
	{ 20, "Allkirjastamiseks või kontrollimiseks lohista fail siia…", 470 },
	{ 22, "Krüpteerimine õnnestus!", 236 },
	{ 24, "MUUDA", 72 },
};
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

#ifndef Q_OS_MAC
		// See http://doc.qt.io/qt-5/highdpi.html
		// and http://doc.qt.io/qt-5/scalability.html
		for (auto sample : condensedSamples)
			condensedMapping[sample.fontSize] = calcFontSize(sample, condensed);
		for (auto sample : regularSamples)
			regularMapping[sample.fontSize] = calcFontSize(sample, regular);
#endif
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
	QFont font(Styles::Font font, int size)
	{
#ifdef Q_OS_MAC
		return QFont(fontName(font), size);
#else
		int adjusted = 0;
		const QMap<int, int> mapping = font == Styles::Condensed ? condensedMapping : regularMapping;

		if (mapping.find(size) != mapping.cend())
			adjusted = mapping[size];
		else
			adjusted = size - (size > FONT_DECREASE_CUTOFF ? FONT_SIZE_DECREASE_LARGE : FONT_SIZE_DECREASE_SMALL);
		return QFont(fontName(font), adjusted);
#endif
	};

private:
#ifndef Q_OS_MAC
	int calcFontSize(const FontSample &sample, const QString &font)
	{
		int fontSize = sample.fontSize;
		int prevWidth = 0;
		int width = 0;

		for(; fontSize >= MIN_FONT_SIZE; fontSize--)
		{
			width = QFontMetrics(QFont(font, fontSize)).width(sample.sample);
			if (width <= sample.width)
				break;
			prevWidth = width;
		}

		if(prevWidth)
		{
			if(abs(width - sample.width) > abs(prevWidth - sample.width))
				fontSize++;
		}
		return fontSize;
	};
#endif

	QString bold;
	QString condensed;
	QString condensedBold;
	QString regular;
#ifndef Q_OS_MAC
	QMap<int, int> condensedMapping;
	QMap<int, int> regularMapping;
#endif
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
