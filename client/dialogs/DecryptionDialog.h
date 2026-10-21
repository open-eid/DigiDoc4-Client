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

	// keys: the recipient's locks, e.g. one per ID-card and Digi-ID of the same person
	explicit DecryptionDialog(CryptoDoc *doc, std::vector<CKey> keys, const QString &recipient,
		QWidget *parent = nullptr);
	~DecryptionDialog() final;
	std::unique_ptr<QCryptoBackend> takeBackend();
	// The lock a card's login (takeBackend()) is for, the one of the recipient's that its card opens
	const CKey* selectedKey() const;

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
	const CKey* keyFor(const TokenData &token) const;
	const CKey* shareKey() const;

	CryptoDoc *cryptoDoc;
	std::unique_ptr<QCryptoBackend> m_backend;
	std::unique_ptr<Overlay> m_overlay;
	std::vector<CKey> m_keys;
	Ui::DecryptionDialog *ui;
	TokenData m_selectedToken;
	bool m_confirmDecrypt = false;
	// The lock's share servers have known authentication and relying party servers
	bool m_sharesKnown = true;
};
