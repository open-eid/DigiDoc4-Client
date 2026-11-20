// SPDX-FileCopyrightText: Estonian Information System Authority
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "widgets/StyledWidget.h"

class TokenData;

class CardPopup : public StyledWidget
{
	Q_OBJECT

public:
	explicit CardPopup(const QVector<TokenData> &cache, QWidget *parent = nullptr);

signals:
	void activated(const TokenData &token);
};
