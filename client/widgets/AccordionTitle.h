// SPDX-FileCopyrightText: Estonian Information System Authority
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include <QCheckBox>

class AccordionTitle final : public QCheckBox
{
	Q_OBJECT

public:
	explicit AccordionTitle(QWidget *parent = nullptr);
};
