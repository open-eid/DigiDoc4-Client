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
	enum Method { IDCard, MobileID, SmartID };

	explicit DecryptionDialog(CryptoDoc *doc, const CKey &key, const QString &recipient,
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
	void updateProgressInfo();
	void updateTabs();
	void updateShareButton();
	bool performLogin();
	bool performShareDecrypt();
	Method currentMethod() const;

	CryptoDoc *cryptoDoc;
	std::unique_ptr<QCryptoBackend> m_backend;
	std::unique_ptr<Overlay> m_overlay;
	CKey m_key;
	Ui::DecryptionDialog *ui;
	TokenData m_selectedToken;
	bool m_confirmDecrypt = false;
	// The lock's share servers have known authentication and relying party servers
	bool m_sharesKnown = true;
};
