// SPDX-FileCopyrightText: Estonian Information System Authority
// SPDX-License-Identifier: LGPL-2.1-or-later

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
