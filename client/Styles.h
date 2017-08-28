#pragma once

#include <memory>

#include <QtCore/QtGlobal>
#include <QFont>

class StylesPrivate;
class Styles
{
public:
	static Styles& instance();

    const QFont& fontRegular();
    const QFont& fontSemiBold();

private:
	explicit Styles();

	Q_DISABLE_COPY(Styles);

	std::unique_ptr<StylesPrivate> d;
};
