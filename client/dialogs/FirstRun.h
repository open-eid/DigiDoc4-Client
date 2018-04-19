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

#include <QDialog>

namespace Ui {
class FirstRun;
}

class QKeyEvent;
class QLabel;

class FirstRun : public QDialog
{
	Q_OBJECT

public:
	explicit FirstRun(QWidget *parent = 0);
	~FirstRun();

protected:
	void keyPressEvent(QKeyEvent *event) override;
	void mouseMoveEvent(QMouseEvent *event) override;
	void mousePressEvent(QMouseEvent *event) override;
	void mouseReleaseEvent(QMouseEvent *event) override;

signals:
	void langChanged(const QString& lang);

private:
	void loadImages();
	void loadPixmap(const QString &base, const QString &lang, QLabel *parent);

	enum View
	{
		Language,
		Intro,
		Signing,
		Encryption,
		MyEid
	};

	Ui::FirstRun *ui;
	bool dragged;
	QPoint lastPos;
};

