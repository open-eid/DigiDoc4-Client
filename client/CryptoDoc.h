/*
 * QDigiDocCrypto
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#pragma once

#include "common_enums.h"
#include "DocumentModel.h"

#include <QtCore/QLoggingCategory>
#include <QtNetwork/QSslCertificate>

#include <functional>
#include <memory>

#include <cdoc/Lock.h>

class QCryptoBackend;

Q_DECLARE_LOGGING_CATEGORY(CRYPTO)

//
// A wrapper structure for UI that contains either:
// - lock information for decryption
// - recipient certificate for encryption
//

struct CKey {
public:
	CKey(const libcdoc::Lock& _lock) : lock(_lock) {}
	CKey(QSslCertificate rcpt_cert);
	// Mobile-ID / Smart-ID recipient, encrypted for through key share servers
	static CKey share(const QString &personalCode);

	libcdoc::Lock lock;
    QSslCertificate rcpt_cert;

	// True when cert is the one this lock is addressed to, i.e. the card
	// holding its private key is the one that can open the container.
	bool canDecrypt(const QSslCertificate &cert) const;
	bool isSupported() const;
	// Personal code of the person the key is addressed to, empty when it is
	// not addressed to a person (password, symmetric or unsupported lock)
	QString personalCode() const;
	bool operator== (const CKey& rhs) const;
};

class CryptoDoc final: public QObject
{
	Q_OBJECT
public:
	CryptoDoc(QObject *parent = nullptr);
	~CryptoDoc() final;

	bool supportsSymmetricKeys() const;
	bool supportsKeyShares() const;
	bool addEncryptionKey(const CKey& key);
	void clear(const QString &file = {}, int version = -1);
	bool decrypt(const libcdoc::Lock &lock, const QByteArray& secret, std::unique_ptr<QCryptoBackend> &&backend);
	// Mobile-ID with phone, Smart-ID without, for a key share lock; onFeedback gets
	// each verification code to show while the user confirms on their device
	bool decrypt(const libcdoc::Lock &lock, const QString &phone, std::function<void(int code)> onFeedback);
	// Ends a Mobile-ID / Smart-ID decryption in progress
	void cancel();
	bool encrypt(const QString &filename = {}, const QString& label = {}, const QByteArray& secret = {});
	DocumentModel* documentModel() const;
	QString fileName() const;
	const std::vector<CKey>& keys() const;
	bool move(const QString &to);
	bool open(const QString &file);
	void removeKeys(const std::vector<CKey> &keys);
	void clearKeys();
	bool saveCopy(const QString &filename);
	ria::qdigidoc4::ContainerState state() const;

private:
	struct Private;
	Private *d;

	friend class CDocumentModel;
};

class CDocumentModel final: public DocumentModel
{
	Q_OBJECT
public:
	bool addFile(const QString &file, const QString &mime = QStringLiteral("application/octet-stream")) final;
	QString data(int row) const final;
	quint64 fileSize(int row) const final;
	void open(int row) final;
	bool removeRow(int row) final;
	int rowCount() const final;
	QString save(int row, const QString &path) const final;

private:
	CDocumentModel(CryptoDoc::Private *doc);
	Q_DISABLE_COPY(CDocumentModel)

	QString containerName() const final;
	QString copy(int row, const QString &dst) const;

	CryptoDoc::Private *d;

	friend class CryptoDoc;
};
