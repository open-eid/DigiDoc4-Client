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

class StylesPrivate
{
public:
    StylesPrivate()
    {
        int idRegular = QFontDatabase::addApplicationFont(":/fonts/OpenSans-Regular.ttf");
        int idSemiBold = QFontDatabase::addApplicationFont(":/fonts/OpenSans-SemiBold.ttf");
    
        regular = QFont(QFontDatabase::applicationFontFamilies(idRegular).at(0));
        semiBold = QFont(QFontDatabase::applicationFontFamilies(idSemiBold).at(0));
#ifdef Q_OS_MAC
        semiBold.setWeight(75);
#endif
    };
    const QFont& fontRegular() {
        return regular;
    };
    const QFont& fontSemiBold() {
        return semiBold;
    };

private:
    QFont regular;
    QFont semiBold;
};

Styles::Styles()
: d(new StylesPrivate) {}

Styles& Styles::instance()
{
	static Styles styles;
	return styles;
}

const QFont& Styles::fontRegular()
{
    return d->fontRegular();
}
const QFont& Styles::fontSemiBold()
{
    return d->fontSemiBold();
}
