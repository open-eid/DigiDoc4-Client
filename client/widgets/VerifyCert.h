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

#include <common/SslCertificate.h>

namespace Ui {
class VerifyCert;
}

class QSvgWidget;

class VerifyCert : public StyledWidget
{
	Q_OBJECT

public:
	explicit VerifyCert( QWidget *parent = nullptr );
	~VerifyCert() override;

	void addBorders();
	void clear();
	void update(QSmartCardData::PinType type, const QSmartCard *smartCard);
	void update(QSmartCardData::PinType type, const SslCertificate &cert);
	void update(bool warning = false);

signals:
	void changePinClicked( bool isForgotPin, bool isBlockedPin );
	void certDetailsClicked( QString link );

public slots:
	void showWarningIcon();

protected:
	void enterEvent( QEvent * event ) override;
	void leaveEvent( QEvent * event ) override;
	void changeEvent(QEvent* event) override;
	void processClickedBtn();
	void processForgotPinLink(const QString &link);
	void processCertDetails(const QString &link);

private:
	void changePinStyle( const QString &background ); 

	Ui::VerifyCert *ui;

	bool isValidCert;
	bool isBlockedPin;
	bool isTempelType;
	QString borders;

	QSvgWidget* greenIcon;
	QSvgWidget* orangeIcon;
	QSvgWidget* redIcon;

	QSmartCardData::PinType pinType;
	QSmartCardData cardData;
	SslCertificate c;
};
