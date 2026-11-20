// SPDX-FileCopyrightText: Estonian Information System Authority
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include <QLabel>

// A label that fades in on the parent, shows a notification and then fades out.
class FadeInNotification final: public QLabel
{
	Q_OBJECT

public:
	using ms = std::chrono::milliseconds;
	enum Type : quint8 {
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
