/*
 * QDigiDoc4
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

#include <QtCore/QAbstractTableModel>

#include <digidocpp/Container.h>
#include <digidocpp/Exception.h>

class DigiDoc;
class QDateTime;
class QSslCertificate;
class QStringList;

class DocumentModel: public QAbstractTableModel
{
	Q_OBJECT
public:
	enum Columns
	{
		Name = 0,
		Mime = 1,
		Size = 2,
		Save = 3,
		Remove = 4,
		Id = 5,

		NColumns
	};

	int columnCount( const QModelIndex &parent = QModelIndex() ) const;
	QVariant data( const QModelIndex &index, int role = Qt::DisplayRole ) const;
	Qt::ItemFlags flags( const QModelIndex &index ) const;
	QMimeData *mimeData( const QModelIndexList &indexes ) const;
	QStringList mimeTypes() const;
	bool removeRows( int row, int count, const QModelIndex &parent = QModelIndex() );
	int rowCount( const QModelIndex &parent = QModelIndex() ) const;
	Qt::DropActions supportedDragActions() const;

	void reset();
	QString save( const QModelIndex &index, const QString &path ) const;

public slots:
	void open( const QModelIndex &index );

private:
	DocumentModel( DigiDoc *doc );
	Q_DISABLE_COPY(DocumentModel)

	DigiDoc *d;

	friend class DigiDoc;
};

class DigiDocSignature
{
public:
	enum SignatureStatus
	{
		Valid,
		Warning,
		NonQSCD,
		Test,
		Invalid,
		Unknown
	};
	enum SignatureWarning
	{
		DigestWeak = 1 << 2
	};
	DigiDocSignature(const digidoc::Signature *signature, const DigiDoc *parent);

	QSslCertificate	cert() const;
	QDateTime	dateTime() const;
	QString		lastError() const;
	QString		location() const;
	QStringList	locations() const;
	QSslCertificate ocspCert() const;
	QByteArray	ocspNonce() const;
	QDateTime	ocspTime() const;
	const DigiDoc *parent() const;
	QString		policy() const;
	QString		profile() const;
	QString		role() const;
	QStringList	roles() const;
	QString		signatureMethod() const;
	QString		signedBy() const;
	QDateTime	signTime() const;
	QString		spuri() const;
	QSslCertificate tsCert() const;
	QDateTime	tsTime() const;
	QSslCertificate tsaCert() const;
	QDateTime	tsaTime() const;
	SignatureStatus validate() const;
	int warning() const;

private:
	void setLastError( const digidoc::Exception &e ) const;
	void parseException( SignatureStatus &result, const digidoc::Exception &e ) const;
	SignatureStatus validate(const std::string &policy) const;
	QDateTime toTime(const std::string &time) const;

	const digidoc::Signature *s;
	mutable QString m_lastError;
	const DigiDoc *m_parent;
	mutable unsigned int m_warning = 0;
};

class DigiDoc: public QObject
{
	Q_OBJECT
public:
	enum DocumentType {
		DDocType,
		BDoc2Type
	};

	explicit DigiDoc( QObject *parent = 0 );
	~DigiDoc();

	void addFile( const QString &file );
	bool addSignature( const QByteArray &signature );
	void create( const QString &file );
	void clear();
	DocumentModel *documentModel() const;
	QString fileName() const;
	bool isNull() const;
	bool isReadOnlyTS() const;
	bool isService() const;
	bool isSupported() const;
	QString mediaType() const;
	QString newSignatureID() const;
	bool open( const QString &file );
	void removeSignature( unsigned int num );
	void save( const QString &filename = QString() );
	bool sign(
		const QString &city,
		const QString &state,
		const QString &zip,
		const QString &country,
		const QString &role,
		const QString &role2 );
	QString signatureFormat() const;
	QList<DigiDocSignature> signatures() const;
	QList<DigiDocSignature> timestamps() const;
	DocumentType documentType() const;
	QByteArray getFileDigest( unsigned int i ) const;

	static bool parseException( const digidoc::Exception &e, QStringList &causes,
		digidoc::Exception::ExceptionCode &code);

private:
	bool checkDoc( bool status = false, const QString &msg = QString() ) const;
	void setLastError( const QString &msg, const digidoc::Exception &e );

	digidoc::Container *b = nullptr, *parentContainer = nullptr;
	QString			m_fileName;
	DocumentModel	*m_documentModel = nullptr;
	QStringList		m_tempFiles;

	friend class DocumentModel;
};
