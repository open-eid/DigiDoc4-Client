// SPDX-FileCopyrightText: Estonian Information System Authority
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "common_enums.h"
#include "DocumentModel.h"

#include <QtCore/QIODevice>
#include <QtCore/QLoggingCategory>
#include <QtNetwork/QSslCertificate>

#include <cdoc/CDoc.h>
#include <cdoc/Certificate.h>
#include <cdoc/Lock.h>
#include <cdoc/Recipient.h>

class QSslKey;

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

	libcdoc::Lock lock;
    QSslCertificate rcpt_cert;

	bool operator== (const CKey& rhs) const;
};

class CryptoDoc final: public QObject
{
	Q_OBJECT
public:
	CryptoDoc(QObject *parent = nullptr);
	~CryptoDoc() final;

	bool supportsSymmetricKeys() const;
	bool addEncryptionKey(const CKey& key);
	bool canDecrypt(const QSslCertificate &cert);
	void clear(const QString &file = {}, int version = -1);
	bool decrypt(const libcdoc::Lock *lock, const QByteArray& secret);
	bool encrypt(const QString &filename = {}, const QString& label = {}, const QByteArray& secret = {});
	DocumentModel* documentModel() const;
	QString fileName() const;
	const std::vector<CKey>& keys() const;
	bool move(const QString &to);
	bool open(const QString &file);
	void removeKey(unsigned int id);
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
	QString mime(int row) const final;
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
