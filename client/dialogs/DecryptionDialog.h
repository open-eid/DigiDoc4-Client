// SPDX-FileCopyrightText: Estonian Information System Authority
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once
#include <QDialog>

#include "CryptoDoc.h"
#include "TokenData.h"

namespace Ui { class DecryptionDialog; }
class Overlay;

class DecryptionDialog final : public QDialog
{
	Q_OBJECT
public:
	explicit DecryptionDialog(const CKey &key, const QString &recipient,
		QWidget *parent = nullptr);
	~DecryptionDialog() final;
	std::unique_ptr<QCryptoBackend> takeBackend();

private:
	void changeEvent(QEvent *event) final;
	void reject() final;
	void updateCards();
	void updateSelection();
	void updatePinControls();
	void updateNoCard();
	bool performLogin();

	std::unique_ptr<QCryptoBackend> m_backend;
	std::unique_ptr<Overlay> m_overlay;
	CKey m_key;
	Ui::DecryptionDialog *ui;
	TokenData m_selectedToken;
};
