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
#include "SslCertificate.h"

namespace Ui {
class VerifyCert;
}

class VerifyCert : public StyledWidget
{
	Q_OBJECT

public:
	explicit VerifyCert( QWidget *parent = nullptr );
	~VerifyCert() final;

	void addBorders();
	void clear();
	void update(QSmartCardData::PinType type, const QSmartCard *smartCard);
	void update(QSmartCardData::PinType type, const SslCertificate &cert);

signals:
	void changePinClicked( bool isForgotPin, bool isBlockedPin );

private:
	void changePinStyle( const QString &background ); 
	bool event(QEvent *event) final;
	void update();

	Ui::VerifyCert *ui;

	bool isValidCert = false;
	bool isBlockedPin = false;
	QString borders;

	QSmartCardData::PinType pinType = QSmartCardData::Pin1Type;
	QSmartCardData cardData;
	SslCertificate c;
};
