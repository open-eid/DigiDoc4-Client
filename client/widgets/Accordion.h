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

#include "QSmartCard.h"

class SslCertificate;

namespace Ui {
class Accordion;
}

class AccordionTitle;

class Accordion final : public QWidget
{
	Q_OBJECT

public:
	explicit Accordion( QWidget *parent = nullptr );
	~Accordion() final;

	void clear();
	void updateInfo(const SslCertificate &info);
	void updateInfo(const QSmartCardData &data);

Q_SIGNALS:
	void changePinClicked(QSmartCardData::PinType, QSmartCard::PinAction);

private:
	void changeEvent(QEvent* event) override;

	Ui::Accordion *ui;
};
