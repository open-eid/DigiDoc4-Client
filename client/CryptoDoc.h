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

#include <memory>

class QSslKey;

struct CKey
{
public:
	enum Type {
        SYMMETRIC_KEY,
        PUBLIC_KEY,
        CERTIFICATE,
		CDOC1,
		SERVER
	};

	enum PKType {
		ECC,
		RSA
	};

	enum DecryptionStatus {
		CANNOT_DECRYPT,
		CAN_DECRYPT,
		NEED_KEY
	};

	Type type;
    QString label;

    // Decryption data
    QByteArray encrypted_fmk;

    // Recipients public key
    // QByteArray key;

    bool isSymmetric() const { return type == Type::SYMMETRIC_KEY; }
    bool isPKI() const { return (type == Type::CERTIFICATE) || (type == Type::CDOC1) || (type == Type::PUBLIC_KEY) || (type == Type::SERVER); }
    bool isCertificate() const { return (type == Type::CERTIFICATE) || (type == Type::CDOC1); }
    bool isCDoc1() const { return type == Type::CDOC1; }

    bool isTheSameRecipient(const CKey &key) const;
    bool isTheSameRecipient(const QSslCertificate &cert) const;

protected:
    CKey(Type _type) : type(_type) {};
private:
    bool operator==(const CKey &other) const { return false; }
};

// Symmetric key (plain or PBKDF)
// Usage:
// CDoc2:encrypt/decrypt

struct CKeySymmetric : public CKey {
    QByteArray salt;
    // PBKDF
    QByteArray pw_salt;
    // 0 symmetric key, >0 password
    int32_t kdf_iter;

    CKeySymmetric(const QByteArray& _salt) : CKey(Type::SYMMETRIC_KEY), salt(_salt), kdf_iter(0) {}
};

// Base PKI key
// Usage:
// CDoc2:encrypt

struct CKeyPKI : public CKey {
    // Recipient's public key
    PKType pk_type;
    QByteArray rcpt_key;

protected:
    CKeyPKI(Type _type) : CKey(_type), pk_type(PKType::ECC) {};
    CKeyPKI(Type _type, PKType _pk_type, QByteArray _rcpt_key) : CKey(_type), pk_type(_pk_type), rcpt_key(_rcpt_key) {};
};


// Public key with additonal information
// Usage:
// CDoc2:encrypt - if recipient is specified by certificate
// CDoc1:encrypt

struct CKeyCert : public CKeyPKI {
    QSslCertificate cert;

    CKeyCert(const QSslCertificate &cert) : CKeyCert(CKey::Type::CERTIFICATE, cert) {};

    void setCert(const QSslCertificate &c);

protected:
    CKeyCert(Type _type) : CKeyPKI(_type) {};
    CKeyCert(Type _type, const QSslCertificate &cert);
};

// CDoc1 decryption key (with additional information from file)
// Usage:
// CDoc1:decrypt

struct CKeyCDoc1 : public CKeyCert {

    QByteArray publicKey;
    QString concatDigest, method;
	QByteArray AlgorithmID, PartyUInfo, PartyVInfo;

    CKeyCDoc1() : CKeyCert(Type::CDOC1) {};
};

// CDoc2 PKI key with key material
// Usage:
// CDoc2: decrypt

struct CKeyPublicKey : public CKeyPKI {
    // Either ECC public key or RSA encrypted kek
    QByteArray key_material;

    CKeyPublicKey() : CKeyPKI(Type::PUBLIC_KEY) {};
    CKeyPublicKey(PKType _pk_type, QByteArray _rcpt_key) : CKeyPKI(Type::PUBLIC_KEY, _pk_type, _rcpt_key) {};
};

// CDoc2 PKI key with server info
// Usage:
// CDoc2: decrypt

struct CKeyServer : public CKeyPKI {
    // Server info
    QString keyserver_id, transaction_id;

	static std::shared_ptr<CKeyServer> fromKey(QByteArray _key, PKType _pk_type);
protected:
    CKeyServer(QByteArray _rcpt_key, PKType _pk_type) : CKeyPKI(Type::SERVER, _pk_type, _rcpt_key) {};
};

class CDoc
{
public:
	struct File
	{
		QString name, id, mime;
		qint64 size;
		std::unique_ptr<QIODevice> data;
	};

	virtual ~CDoc() = default;
	// Return affirmative if keys match or NEED_KEY if document includes symmetric key */
	virtual CKey::DecryptionStatus canDecrypt(const QSslCertificate &cert) const = 0;
	virtual std::shared_ptr<CKey> getDecryptionKey(const QSslCertificate &cert) const = 0;
	virtual bool decryptPayload(const QByteArray &fmk) = 0;
	virtual bool save(const QString &path) = 0;
	bool setLastError(const QString &msg) { return (lastError = msg).isEmpty(); }
    virtual QByteArray getFMK(const CKey &key, const QByteArray& secret) = 0;
	virtual int version() = 0;

	QList<std::shared_ptr<CKey>> keys;
	std::vector<File> files;
	QString lastError;
};

class CryptoDoc final: public QObject
{
	Q_OBJECT
public:
	CryptoDoc(QObject *parent = nullptr);
	~CryptoDoc() final;

	bool addKey(std::shared_ptr<CKey> key );
	bool canDecrypt(const QSslCertificate &cert);
	void clear(const QString &file = {});
    bool decrypt(std::shared_ptr<CKey> key, const QByteArray& secret);
	DocumentModel* documentModel() const;
	bool encrypt(const QString &filename = {});
    bool encryptLT(const QString& label, const QByteArray& secret, unsigned int kdf_iter);
    QString fileName() const;
	QList<std::shared_ptr<CKey>> keys() const;
	bool move(const QString &to);
	bool open( const QString &file );
	void removeKey( int id );
	bool saveCopy(const QString &filename);
	ria::qdigidoc4::ContainerState state() const;

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
