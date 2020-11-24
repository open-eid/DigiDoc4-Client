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

#include "widgets/StyledWidget.h"

#include "QSmartCard.h"

namespace Ui {
class InfoStack;
}

class SslCertificate;

class InfoStack final: public StyledWidget
{
	Q_OBJECT

public:
	explicit InfoStack( QWidget *parent = nullptr );
	~InfoStack() final;

	void clearData();
	void update(const SslCertificate &cert);
	void update(const QSmartCardData &t);
	void showPicture(const QPixmap &pixmap);

signals:
	void photoClicked(const QPixmap &pixmap);

protected:
	void changeEvent(QEvent* event) final;
	bool eventFilter(QObject *o, QEvent *e) final;

private:
	void update();
	void clearPicture();

	Ui::InfoStack *ui;

	QSmartCardData data;
	int certType = 0;
	QString citizenshipText;
	QString givenNamesText;
	QString personalCodeText;
	QString serialNumberText;
	QString surnameText;
	const char *pictureText = "DOWNLOAD";
};
