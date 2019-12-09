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

#include <QtCore/QStringList>
#include <QtNetwork/QSslCertificate>

class CKey
{
public:
	CKey() = default;
	CKey( const QSslCertificate &cert ) { setCert( cert ); }
	void setCert( const QSslCertificate &cert );
	bool operator==( const CKey &other ) const { return other.cert == cert; }

	QSslCertificate cert;
	QString id, name, recipient, method, agreement, derive, concatDigest;
	QByteArray AlgorithmID, PartyUInfo, PartyVInfo;
	QByteArray cipher, publicKey;
};

class CryptoDoc: public QObject
{
	Q_OBJECT
public:
	CryptoDoc(QObject *parent = nullptr);
	~CryptoDoc() final;

	bool addKey( const CKey &key );
	bool canDecrypt(const QSslCertificate &cert);
	void clear( const QString &file = QString() );
	bool decrypt();
	DocumentModel* documentModel() const;
	bool encrypt( const QString &filename = QString() );
	QString fileName() const;
	QList<QString> files();
	bool isEncrypted() const;
	bool isNull() const;
	bool isSigned() const;
	QList<CKey> keys();
	bool move(const QString &to);
	bool open( const QString &file );
	void removeKey( int id );
	bool saveCopy(const QString &filename);
	bool saveDDoc( const QString &filename );
	ria::qdigidoc4::ContainerState state() const;

	static QByteArray concatKDF(const QString &digestMethod,
		quint32 keyDataLen, const QByteArray &z, const QByteArray &otherInfo);

private:
	class Private;
	Private *d;
	ria::qdigidoc4::ContainerState containerState;

	friend class CDocumentModel;
};

class CDocumentModel: public DocumentModel
{
	Q_OBJECT
public:
	bool addFile(const QString &file, const QString &mime = QStringLiteral("application/octet-stream")) override;
	void addTempReference(const QString &file) override;
	QString data(int row) const override;
	QString fileSize(int row) const override;
	QString mime(int row) const override;
	bool removeRows(int row, int count) override;
	int rowCount() const override;
	QString save(int row, const QString &path) const override;

public slots:
	void open(int row) override;

private:
	CDocumentModel(CryptoDoc::Private *doc);
	Q_DISABLE_COPY(CDocumentModel)

	QString copy(int row, const QString &dst) const;

	CryptoDoc::Private *d;

	friend class CryptoDoc;
};
