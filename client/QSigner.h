// SPDX-FileCopyrightText: Estonian Information System Authority
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include <QtCore/QThread>
#include <digidocpp/crypto/Signer.h>

#include <QCryptographicHash>

class QReadWriteLock;
class QSmartCard;
class TokenData;

class QSigner final: public QThread, public digidoc::Signer
{
	Q_OBJECT

public:
	explicit QSigner(QObject *parent = nullptr);
	~QSigner() final;

	QList<TokenData> cache() const;
	digidoc::X509Cert cert() const final;
	void selectCard(const TokenData &token);
	std::vector<unsigned char> sign( const std::string &method,
		const std::vector<unsigned char> &digest) const final;
	QReadWriteLock& sessionLock() const;
	QSmartCard * smartcard() const;
	TokenData tokenauth() const;
	TokenData tokensign() const;
	QString getLastErrorStr() const;

Q_SIGNALS:
	void cacheChanged();
	void authDataChanged( const TokenData &token );
	void signDataChanged( const TokenData &token );
	void error(const QString &title, const QString &text);

private:
	static QCryptographicHash::Algorithm methodToNID(const std::string &method);
	void run() final;

	class Private;
	Private *d;
};
