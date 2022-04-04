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

class CKey
{
public:
	CKey() = default;
	CKey(const QByteArray &_key, bool _isRSA): key(_key), isRSA(_isRSA) {}
	CKey(const QSslCertificate &cert);
	bool operator==(const CKey &other) const { return other.key == key; }

	void setCert(const QSslCertificate &c);

	QByteArray key, cipher, publicKey;
	QSslCertificate cert;
	bool isRSA = false;
	QString recipient;
	// CDoc1
	QString agreement, concatDigest, derive, method, id, name;
	QByteArray AlgorithmID, PartyUInfo, PartyVInfo;
	// CDoc2
	QByteArray encrypted_kek;
	QString keyserver_id, transaction_id;
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
	virtual CKey canDecrypt(const QSslCertificate &cert) const = 0;
	virtual bool decryptPayload(const QByteArray &key) = 0;
	virtual bool save(const QString &path) = 0;
	bool setLastError(const QString &msg) { return (lastError = msg).isEmpty(); }
	virtual QByteArray transportKey(const CKey &key) = 0;
	virtual int version() = 0;

	QList<CKey> keys;
	std::vector<File> files;
	QString lastError;
};

class CryptoDoc final: public QObject
{
	Q_OBJECT
public:
	CryptoDoc(QObject *parent = nullptr);
	~CryptoDoc() final;

	bool addKey( const CKey &key );
	bool canDecrypt(const QSslCertificate &cert);
	void clear(const QString &file = {});
	bool decrypt();
	DocumentModel* documentModel() const;
	bool encrypt(const QString &filename = {});
	QString fileName() const;
	QList<QString> files();
	QList<CKey> keys() const;
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
	bool removeRow(int row) final;
	int rowCount() const final;
	QString save(int row, const QString &path) const final;

public slots:
	void open(int row) final;

private:
	CDocumentModel(CryptoDoc::Private *doc);
	Q_DISABLE_COPY(CDocumentModel)

	QString copy(int row, const QString &dst) const;

	CryptoDoc::Private *d;

	friend class CryptoDoc;
};
