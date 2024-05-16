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

#include <QLabel>

// A label that fades in on the parent, shows a notification and then fades out.
class FadeInNotification final: public QLabel
{
	Q_OBJECT

public:
	using ms = std::chrono::milliseconds;
	enum Type {
		Success,
		Warning,
		Error,
		Default,
		None,
	};
	explicit FadeInNotification(QWidget *parent, QRect rect, Type type = None, const QString &label = {});
	void start(ms fadeInTime = ms(750));

	static void success(QWidget *parent, const QString &label);
	static void warning(QWidget *parent, const QString &label);
	static void error(QWidget *parent, const QString &label, int height = 65);

private:
	bool eventFilter(QObject *watched, QEvent *event) final;
};
