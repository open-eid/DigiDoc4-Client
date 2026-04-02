// SPDX-FileCopyrightText: Estonian Information System Authority
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include <QDialog>

class RoleAddressDialog final : public QDialog
{
	Q_OBJECT
public:
	explicit RoleAddressDialog(QWidget *parent = nullptr);
	~RoleAddressDialog() final;
	int get(QString &city, QString &country, QString &state, QString &zip, QString &role);

private:
	class Private;
	Private *d;
};
