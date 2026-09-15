// SPDX-FileCopyrightText: Estonian Information System Authority
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include <QLineEdit>

class LineEdit: public QLineEdit
{
	Q_OBJECT
public:
	Q_PROPERTY(QString label READ label WRITE setLabel FINAL)

	explicit LineEdit(QWidget *parent = nullptr);

	QString label() const;
	void setLabel(QString _label);

private:
	void paintEvent(QPaintEvent *event) override;

	QString _label;
	QString placeholder;
};
