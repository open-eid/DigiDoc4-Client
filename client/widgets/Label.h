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
