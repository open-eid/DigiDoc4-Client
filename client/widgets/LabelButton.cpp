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

#include "LabelButton.h"
#include "Styles.h"

#include <QEvent>

LabelButton::LabelButton( QWidget *parent ): QToolButton(parent)
{}

void LabelButton::init(Style style, const QString &label)
{
	setText(label);
	setFont(Styles::font(Styles::Condensed, 12));
	if(!label.isEmpty())
		setAccessibleName(label.toLower());

	// Blues
	static const QString CURIOUS_BLUE = QStringLiteral("#31A3D9");
	static const QString DEEP_CERULEAN = QStringLiteral("#006EB5");
	// Reds
	static const QString MOJO = QStringLiteral("#981E32");
	// Whites
	static const QString PORCELAIN = QStringLiteral("#f4f5f6");
	static const QString WHITE = QStringLiteral("#ffffff");

	static const QString borderRadius = QStringLiteral(" border-radius: 2px;");
	static const QString none = QStringLiteral("none");
	static const QString solid = QStringLiteral("1px solid %1");
	static const QString styleTemplate = QStringLiteral("background-color: %1; color: %2;%4 border: %3; text-decoration: none solid;");
	auto setStyle = [this](const QString &normal, const QString &hover, const QString &pressed) {
		setStyleSheet(QStringLiteral("QToolButton { %1 }\nQToolButton:hover { %2 }\nQToolButton:pressed { %3 }").arg(normal, hover, pressed));
	};
	switch( style )
	{
	case BoxedDeepCerulean:
		setStyle(
			styleTemplate.arg(PORCELAIN, DEEP_CERULEAN, none, borderRadius),
			styleTemplate.arg(PORCELAIN, DEEP_CERULEAN, solid.arg(DEEP_CERULEAN), borderRadius),
			styleTemplate.arg(DEEP_CERULEAN, PORCELAIN, none, borderRadius));
		break;
	case BoxedMojo:
		setStyle(
			styleTemplate.arg(PORCELAIN, MOJO, none, borderRadius),
			styleTemplate.arg(PORCELAIN, MOJO, solid.arg(MOJO), borderRadius),
			styleTemplate.arg(MOJO, PORCELAIN, none, borderRadius));
		break;
	case BoxedDeepCeruleanWithCuriousBlue: // Edit
		setStyle(
			styleTemplate.arg(PORCELAIN, DEEP_CERULEAN, solid.arg(PORCELAIN), borderRadius),
			styleTemplate.arg(PORCELAIN, CURIOUS_BLUE, solid.arg(CURIOUS_BLUE), borderRadius),
			styleTemplate.arg(CURIOUS_BLUE, PORCELAIN, solid.arg(CURIOUS_BLUE), borderRadius));
		break;
	case DeepCeruleanWithLochmara: // Add files
		setStyle(
			styleTemplate.arg(WHITE, DEEP_CERULEAN, none, QString()),
			styleTemplate.arg(DEEP_CERULEAN, WHITE, none, QString()),
			styleTemplate.arg(CURIOUS_BLUE, WHITE, none, QString()));
		break;
	default: break;
	}
}

void LabelButton::setIcons(const QString &normalIcon, const QString &hoverIcon, const QString &pressedIcon, int w, int h)
{
	setIconSize(QSize(w, h));
	normal = ":" + normalIcon;
	hover = ":" + hoverIcon;
	pressed = ":" + pressedIcon;
	QIcon ico;
	ico.addFile(normal, iconSize());
	setIcon(ico);
}

bool LabelButton::event(QEvent *e)
{
	if(icon().isNull())
		return QToolButton::event(e);
	auto set = [this](const QString &icon) {
		if(icon.isEmpty())
			return;
		QIcon ico;
		ico.addFile(icon, iconSize());
		setIcon(ico);
	};
	switch(e->type())
	{
	case QEvent::HoverEnter:
		set(hover);
		break;
	case QEvent::HoverLeave:
		set(normal);
		break;
	case QEvent::MouseButtonPress:
		set(pressed);
		break;
	default: break;
	}
	return QToolButton::event(e);
}
