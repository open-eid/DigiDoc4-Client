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

#include <QWidget>
#include <QPainter>

namespace Ui {
class VerifyCert;
}

class VerifyCert : public QWidget
{
	Q_OBJECT

public:
	explicit VerifyCert(QWidget *parent = 0);
	~VerifyCert();

	void update(bool isValid, const QString &name, const QString &validUntil, const QString &change, const QString &forgot_PIN_HTML = "", const QString &details_HTML = "", const QString &error = "");
	void addBorders();

signals:
    void changePinClicked();

protected:
	void paintEvent(QPaintEvent *) override;

	void enterEvent(QEvent * event) override;
	void leaveEvent(QEvent * event) override;

private:
	Ui::VerifyCert *ui;
	bool isValid;
	QString borders;
};
