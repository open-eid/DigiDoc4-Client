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
		CDOC1,
		SYMMETRIC_KEY,
		PUBLICKEY,
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
	PKType pk_type;

	bool operator==(const CKey &other) const { return other.key == key; }

	QByteArray key, cipher, publicKey;

protected:
	CKey(Type _type) : type(_type), pk_type(PKType::ECC) {};
	CKey(Type _type, PKType _pk_type, QByteArray _key): type(_type), pk_type(_pk_type), key(std::move(_key)) {}
};

// CDoc1 key

struct CKeyCD1 : public CKey {
	QString agreement, concatDigest, derive, method, id, name;
	QByteArray AlgorithmID, PartyUInfo, PartyVInfo;
	QSslCertificate cert;
	QString recipient;

	void setCert(const QSslCertificate &c);

	static std::shared_ptr<CKeyCD1> newEmpty();
	static std::shared_ptr<CKeyCD1> fromKey(QByteArray _key, PKType _pk_type);
	static std::shared_ptr<CKeyCD1> fromCertificate(const QSslCertificate &cert);

	static bool isCDoc1Key(const CKey& key) { return key.type == Type::CDOC1; }
protected:
	CKeyCD1() : CKey(Type::CDOC1) {};
	CKeyCD1(QByteArray _key, PKType _pk_type) : CKey(Type::CDOC1, _pk_type, _key) {}
	CKeyCD1(const QSslCertificate &cert);
};

struct CKeyCD2 : public CKey {
	QString label;
	CKeyCD2(Type type) : CKey(type) {};
	CKeyCD2(Type _type, PKType _pk_type, QByteArray _key): CKey(_type, _pk_type, _key) {}

	static bool isCDoc2Key(const CKey& key) { return (key.type == Type::SYMMETRIC_KEY) || (key.type == Type::PUBLICKEY) || (key.type == Type::SERVER); }
};

// Symmetric key (plain or PBKDF)

struct CKeySymmetric : public CKeyCD2 {
	QByteArray salt;
	// PBKDF
	QByteArray pw_salt;
    int32_t kdf_iter; // 0 symmetric key, >0 password

	CKeySymmetric(const QByteArray& _salt) : CKeyCD2(Type::SYMMETRIC_KEY), salt(_salt), kdf_iter(0) {}
};

struct CKeyPK : public CKeyCD2 {
	QByteArray encrypted_kek;

	CKeyPK() : CKeyCD2(Type::PUBLICKEY) {};
	CKeyPK(PKType _pk_type, QByteArray _key) : CKeyCD2(Type::PUBLICKEY, _pk_type, _key) {};
};

struct CKeyServer : public CKeyCD2 {
	QString keyserver_id, transaction_id;

	static std::shared_ptr<CKeyServer> fromKey(QByteArray _key, PKType _pk_type);
protected:
	CKeyServer(QByteArray _key, PKType _pk_type) : CKeyCD2(Type::SERVER, _pk_type, _key) {};
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
