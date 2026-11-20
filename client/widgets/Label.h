// SPDX-FileCopyrightText: Estonian Information System Authority
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include <QLabel>

class Label : public QLabel {
	Q_OBJECT
 public:
	Q_PROPERTY(QString label READ label WRITE setLabel FINAL)
	Q_PROPERTY(bool fitToParentWidth READ fitToParentWidth WRITE setFitToParentWidth FINAL)
	Q_PROPERTY(int wrapAtWidth READ wrapAtWidth WRITE setWrapAtWidth FINAL)

	explicit Label(QWidget *parent = {});

	QString label() const;
	void setLabel(QString label);
	bool fitToParentWidth() const;
	void setFitToParentWidth(bool enabled);
	int wrapAtWidth() const;
	void setWrapAtWidth(int width);

signals:
	void wordWrapChanged(bool wordWrap);

protected:
	void changeEvent(QEvent *event) override;
	bool eventFilter(QObject *watched, QEvent *event) override;

private:
	void fitToWidth(int availableWidth);
	void updateFit();
	void updateParentEventFilter();

	QString _label;
	QString measuredText;
	int naturalWidth = -1;
	int maximumWrapWidth = 0;
	bool fitParentWidth = false;
};
