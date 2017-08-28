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
