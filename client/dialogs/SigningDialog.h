// SPDX-FileCopyrightText: Estonian Information System Authority
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once
#include <QDialog>

#include "TokenData.h"

class DigiDoc;
namespace Ui { class SigningDialog; }

class SigningDialog final : public QDialog
{
	Q_OBJECT
public:
	enum Method { IDCard, MobileID, SmartID };

	explicit SigningDialog(DigiDoc *doc, QWidget *parent = nullptr);
	~SigningDialog() final;

	void setRoleAddress(const QString &city, const QString &country,
		const QString &state, const QString &zip, const QString &role);

private:
	void updateCards();
	bool performSign();
	Method currentMethod() const;
	QString currentCode() const;

	DigiDoc *digiDoc;
	Ui::SigningDialog *ui;
	TokenData m_selectedToken;
	QString m_role, m_city, m_state, m_country, m_zip;
};
