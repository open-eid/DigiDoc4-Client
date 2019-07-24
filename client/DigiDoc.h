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

#include "common_enums.h"
#include "DocumentModel.h"

#include <digidocpp/Container.h>
#include <digidocpp/Exception.h>

#include <memory>

class DigiDoc;
class QDateTime;
class QSslCertificate;
class QStringList;

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
	QString		id() const;
	QString		lastError() const;
	QString		location() const;
	QStringList	locations() const;
	QByteArray	messageImprint() const;
	QSslCertificate ocspCert() const;
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


class SDocumentModel: public DocumentModel
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
	SDocumentModel(DigiDoc *container);
	Q_DISABLE_COPY(SDocumentModel)

	DigiDoc *doc;
	friend class DigiDoc;
};


class DigiDoc: public QObject
{
	Q_OBJECT
public:
	enum DocumentType {
		DDocType,
		BDoc2Type
	};

	enum Operation {
		Saving,
		Signing,
		Validation
	};

	explicit DigiDoc(QObject *parent = nullptr);
	~DigiDoc();

	bool addFile( const QString &file, const QString &mime );
	void create( const QString &file );
	void clear();
	DocumentModel *documentModel() const;
	QString fileName() const;
	bool isNull() const;
	bool isReadOnlyTS() const;
	bool isService() const;
	bool isModified() const;
	bool isSupported() const;
	QString mediaType() const;
	bool move(const QString &to);
	QString newSignatureID() const;
	bool open( const QString &file );
	void removeSignature( unsigned int num );
	bool save( const QString &filename = QString() );
	bool saveAs(const QString &filename);
	bool sign(const QString &city,
		const QString &state,
		const QString &zip,
		const QString &country,
		const QString &role,
		digidoc::Signer *signer);
	QList<DigiDocSignature> signatures() const;
	ria::qdigidoc4::ContainerState state();
	QList<DigiDocSignature> timestamps() const;
	DocumentType documentType() const;

	static bool parseException( const digidoc::Exception &e, QStringList &causes,
		digidoc::Exception::ExceptionCode &code);

signals:
	void operation(Operation op, bool started);

private:
	bool checkDoc( bool status = false, const QString &msg = QString() ) const;
	void setLastError( const QString &msg, const digidoc::Exception &e );

	std::unique_ptr<digidoc::Container> b;
	std::unique_ptr<digidoc::Container> parentContainer;
	std::unique_ptr<DocumentModel>		m_documentModel;

	ria::qdigidoc4::ContainerState containerState;
	bool			modified = false;
	QString			m_fileName;
	QStringList		m_tempFiles;

	friend class SDocumentModel;
};
