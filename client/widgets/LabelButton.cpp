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
	// Whites
	static const QString WHITE = QStringLiteral("#ffffff");

	static const QString styleTemplate = QStringLiteral("background-color: %1; color: %2; border: none; text-decoration: none solid;");
	auto setStyle = [this](const QString &normal, const QString &hover, const QString &pressed) {
		setStyleSheet(QStringLiteral("QToolButton { %1 }\nQToolButton:hover { %2 }\nQToolButton:pressed { %3 }").arg(normal, hover, pressed));
	};
	switch( style )
	{
	case DeepCeruleanWithLochmara: // Add files
		setStyle(
			styleTemplate.arg(WHITE, DEEP_CERULEAN),
			styleTemplate.arg(DEEP_CERULEAN, WHITE),
			styleTemplate.arg(CURIOUS_BLUE, WHITE));
		break;
	default: break;
	}
}
