// SPDX-FileCopyrightText: Estonian Information System Authority
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include <QDialog>

class CKey;
class KeyDialog final: public QDialog
{
	Q_OBJECT

public:
	KeyDialog(const CKey &key, QWidget *parent = nullptr);
};
