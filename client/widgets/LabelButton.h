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

#pragma once

#include <QtWidgets/QToolButton>

// A label with pre-defined styles that acts as a button.
// LabelButton changes style (colors) on hover and when pressed.
class LabelButton : public QToolButton
{
	Q_OBJECT

public:
	enum Style {
		BoxedDeepCerulean,
		BoxedMojo,
		BoxedDeepCeruleanWithCuriousBlue, // Edit
		DeepCeruleanWithLochmara, // Add files
		White,
		None
	};

	explicit LabelButton(QWidget *parent = nullptr);

	void init( Style style, const QString &label, int code );
	void setIcons(const QString &normalIcon, const QString &hoverIcon, const QString &pressedIcon, int w, int h);
	void clear();
	const QPixmap pixmap();
	void setPixmap(const QPixmap &pixmap);

signals:
	void clicked(int code);

private:
	bool event(QEvent *e) override;

	QString normal, hover, pressed;
	QMetaObject::Connection connection;
};
