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

#ifdef Q_OS_MAC
   #define FONT_SIZE_DECREASE 0
#else
   #define FONT_SIZE_DECREASE 4 // https://forum.qt.io/topic/26663/different-os-s-different-font-sizes/3
#endif

class StylesPrivate
{
public:
    StylesPrivate()
    {
        int idRegular = QFontDatabase::addApplicationFont(":/fonts/OpenSans-Regular.ttf");
        int idSemiBold = QFontDatabase::addApplicationFont(":/fonts/OpenSans-SemiBold.ttf");
    
        regular = QFontDatabase::applicationFontFamilies(idRegular).at(0);
        semiBold = QFontDatabase::applicationFontFamilies(idSemiBold).at(0);
    };
    QFont font(Styles::Font font, int size) {
        QFont f((font == Styles::OpenSansRegular ? regular : semiBold), size - FONT_SIZE_DECREASE);
#ifdef Q_OS_MAC
        f.setWeight(QFont::DemiBold);
#endif
        return f;
    };

private:
    QString regular;
    QString semiBold;
};

Styles::Styles()
: d(new StylesPrivate) {}

Styles& Styles::instance()
{
    static Styles styles;
    return styles;
}

QFont Styles::font(Styles::Font font, int size)
{
    return d->font(font, size);
}
