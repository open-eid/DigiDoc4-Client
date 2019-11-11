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
struct QCardInfo;

class Accordion : public StyledWidget
{
	Q_OBJECT

public:
	explicit Accordion( QWidget *parent = nullptr );
	~Accordion() final;

	void init();
	void clear();
	void closeOtherSection( AccordionTitle* opened );
	QString getEmail();
	void open(AccordionTitle* opened);
	void setFocusToEmail();
	void updateInfo(const QCardInfo &info);
	void updateInfo( const QSmartCard *smartCard );
	bool updateOtherData(const QByteArray &data);

protected:
	void changeEvent(QEvent* event) override;

signals:
	void checkEMail();
	void activateEMail();
	void changePin1Clicked(bool isForgotPin, bool isBlockedPin);
	void changePin2Clicked(bool isForgotPin, bool isBlockedPin);
	void changePukClicked();

private:
	Ui::Accordion *ui;
	AccordionTitle* openSection = nullptr;
};
