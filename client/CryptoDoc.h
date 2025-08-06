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

#include <QtCore/QIODevice>
#include <QtNetwork/QSslCertificate>

#include <cdoc/CDoc.h>
#include <cdoc/Certificate.h>
#include <cdoc/Lock.h>
#include <cdoc/Recipient.h>

class QSslKey;

//
// A wrapper structure for UI that contains either:
// - lock information for decryption
// - recipient certificate for encryption
//

struct CDKey {
    libcdoc::Lock lock;
    QSslCertificate rcpt_cert;
	bool operator== (const CDKey& rhs) const = default;
};

class CryptoDoc final: public QObject
{
	Q_OBJECT
public:
	CryptoDoc(QObject *parent = nullptr);
	~CryptoDoc() final;

	bool supportsSymmetricKeys() const;
    bool addEncryptionKey(const QSslCertificate& cert);
	bool canDecrypt(const QSslCertificate &cert);
	void clear(const QString &file = {});
	bool decrypt(const libcdoc::Lock *lock, const QByteArray& secret);
	bool encrypt(const QString &filename = {}, const QString& label = {}, const QByteArray& secret = {}, uint32_t kdf_iter = 0);
	DocumentModel* documentModel() const;
	QString fileName() const;
	const std::vector<CDKey>& keys() const;
	bool move(const QString &to);
	bool open(const QString &file);
	void removeKey(unsigned int id);
	void clearKeys();
	bool saveCopy(const QString &filename);
	ria::qdigidoc4::ContainerState state() const;

	static std::string labelFromCertificate(const std::vector<uint8_t>& cert);
private:
	class Private;
	Private *d;

	friend class CDocumentModel;
};

class CDocumentModel final: public DocumentModel
{
	Q_OBJECT
public:
	bool addFile(const QString &file, const QString &mime = QStringLiteral("application/octet-stream")) final;
	void addTempReference(const QString &file) final;
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

	QString copy(int row, const QString &dst) const;

	CryptoDoc::Private *d;

	friend class CryptoDoc;
};
