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

class DigiDocSignature
{
public:
	enum SignatureStatus
	{
		Valid,
		Warning,
		NonQSCD,
		Invalid,
		Unknown
	};
	enum SignatureWarning
	{
		DigestWeak = 1 << 2
	};
	DigiDocSignature(const digidoc::Signature *signature = {}, const DigiDoc *parent = {}, bool isTimeStamped = false);

	QSslCertificate	cert() const;
	QDateTime	claimedTime() const;
	const DigiDoc *container() const;
	bool		isInvalid() const;
	QString		lastError() const;
	QString		location() const;
	QStringList	locations() const;
	QByteArray	messageImprint() const;
	QSslCertificate ocspCert() const;
	QDateTime	ocspTime() const;
	QString		policy() const;
	QString		profile() const;
	QString		role() const;
	QStringList	roles() const;
	QString		signatureMethod() const;
	QString		signedBy() const;
	SignatureStatus status() const;
	QString		spuri() const;
	QDateTime	trustedTime() const;
	QSslCertificate tsCert() const;
	QDateTime	tsTime() const;
	QSslCertificate tsaCert() const;
	QDateTime	tsaTime() const;
	int warning() const;

private:
	SignatureStatus status(const digidoc::Exception &e);
	SignatureStatus validate(bool qscd = true);
	static QSslCertificate toCertificate(const std::vector<unsigned char> &der) ;
	static QDateTime toTime(const std::string &time) ;

	const digidoc::Signature *s;
	QString m_lastError;
	const DigiDoc *m_parent;
	unsigned int m_warning = 0;
	bool m_isTimeStamped;
	SignatureStatus m_status;
};


class SDocumentModel final: public DocumentModel
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
	explicit DigiDoc(QObject *parent = {});
	~DigiDoc();

	bool addFile( const QString &file, const QString &mime );
	void create( const QString &file );
	void clear();
	DocumentModel *documentModel() const;
	QString fileName() const;
	bool isAsicS() const;
	bool isCades() const;
	bool isPDF() const;
	bool isModified() const;
	bool isSupported() const;
	QString mediaType() const;
	bool move(const QString &to);
	bool open( const QString &file );
	void removeSignature( unsigned int num );
	bool save(const QString &filename = {});
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

	static QStringList parseException(const digidoc::Exception &e,
		digidoc::Exception::ExceptionCode &code);

private:
	bool isError(bool failure, const QString &msg = {}) const;
	static void setLastError( const QString &msg, const digidoc::Exception &e );

	std::unique_ptr<digidoc::Container> b;
	std::unique_ptr<digidoc::Container> parentContainer;
	std::unique_ptr<DocumentModel>		m_documentModel;

	ria::qdigidoc4::ContainerState containerState = ria::qdigidoc4::UnsignedContainer;
	QList<DigiDocSignature> m_signatures, m_timestamps;
	bool			modified = false;
	QString			m_fileName;
	QStringList		m_tempFiles;

	friend class DigiDocSignature;
	friend class SDocumentModel;
};
