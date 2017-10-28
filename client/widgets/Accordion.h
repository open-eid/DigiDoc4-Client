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
class Accordion;
}

class AccordionTitle;

class Accordion : public StyledWidget
{
	Q_OBJECT

public:
	explicit Accordion( QWidget *parent = nullptr );
	~Accordion();

	void init();
	void closeOtherSection( AccordionTitle* opened );
	void updateOtherData( bool activate, const QString &eMail = "", const quint8 &errorCode = 0 );
	QString getEmail();
	void setFocusToEmail();
	void updateInfo( const QSmartCard *smartCard );
	void clearOtherEID();
	void updateMobileIdInfo();
	void updateDigiIdInfo();
	void idCheckOtherEIdNeeded( AccordionTitle* opened );

protected:
	void changeEvent(QEvent* event) override;

private Q_SLOTS:
	void changePin1( bool isForgotPin, bool isBlockedPin );
	void changePin2( bool isForgotPin, bool isBlockedPin );
	void changePuk( bool isForgotPuk, bool isBlockedPin );
	void certDetails( const QString &link );

signals:
	void checkEMail();
	void activateEMail();
	void changePin1Clicked( bool isForgotPin, bool isBlockedPin );
	void changePin2Clicked( bool isForgotPin, bool isBlockedPin );
	void changePukClicked( bool isForgotPuk );
	void certDetailsClicked( const QString &link );
	void checkOtherEID();

private:
	Ui::Accordion *ui;
	AccordionTitle* openSection;
};
