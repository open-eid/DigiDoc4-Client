// SPDX-FileCopyrightText: Estonian Information System Authority
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "widgets/StyledWidget.h"

#include "QSmartCard.h"
#include "SslCertificate.h"

namespace Ui { class VerifyCert; }

class VerifyCert final : public StyledWidget
{
	Q_OBJECT

public:
	explicit VerifyCert( QWidget *parent = nullptr );
	~VerifyCert() final;

	void clear();
	void update(QSmartCardData::PinType type, const QSmartCardData &data);
	void update(QSmartCardData::PinType type, SslCertificate cert);

signals:
	void changePinClicked(QSmartCard::PinAction);

private:
	void changeEvent(QEvent *event) final;
	void update();

	Ui::VerifyCert *ui;

	QSmartCardData::PinType pinType = QSmartCardData::Pin1Type;
	QSmartCardData cardData;
	SslCertificate c;
};
