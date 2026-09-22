// SPDX-FileCopyrightText: Estonian Information System Authority
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include <QLabel>

class Label : public QLabel {
	Q_OBJECT
 public:
	Q_PROPERTY(QString label READ label WRITE setLabel FINAL)

	explicit Label(QWidget *parent = {});

	QString label() const;
	void setLabel(QString _label);

private:
	QString _label;
};
