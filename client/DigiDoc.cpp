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

#include "DigiDoc.h"

#include "Application.h"
#include "QSigner.h"
#include "SslCertificate.h"
#include "dialogs/FileDialog.h"
#include "dialogs/WarningDialog.h"

#include <common/Settings.h>
#include <common/TokenData.h>

#include <digidocpp/DataFile.h>
#include <digidocpp/Signature.h>
#include <digidocpp/crypto/X509Cert.h>

#include <QtCore/QDateTime>
#include <QtCore/QFileInfo>
#include <QtCore/QProcessEnvironment>
#include <QtCore/QStringList>
#include <QtCore/QUrl>
#include <QtGui/QDesktopServices>

using namespace digidoc;
using namespace ria::qdigidoc4;

static std::string to( const QString &str ) { return std::string( str.toUtf8().constData() ); }
static QString from( const std::string &str ) { return QString::fromUtf8( str.c_str() ).normalized( QString::NormalizationForm_C ); }
static QByteArray fromVector( const std::vector<unsigned char> &d )
{ return QByteArray((const char *)d.data(), int(d.size())); }



class OpEmitter
{
public:
	OpEmitter(DigiDoc *digiDoc, DigiDoc::Operation operation) : doc(digiDoc), op(operation) 
	{
		emit doc->operation(op, true);
	}
	~OpEmitter()
	{
		emit doc->operation(op, false);
	}

private:
	DigiDoc *doc;
	DigiDoc::Operation op;
};



DigiDocSignature::DigiDocSignature(const digidoc::Signature *signature, const DigiDoc *parent)
:	s(signature)
,	m_parent(parent)
{}

QSslCertificate DigiDocSignature::cert() const
{
	try
	{
		return QSslCertificate( fromVector(s->signingCertificate()), QSsl::Der );
	}
	catch( const Exception & ) {}
	return QSslCertificate();
}

QDateTime DigiDocSignature::dateTime() const
{
	return toTime(s->trustedSigningTime());
}

QString DigiDocSignature::id() const
{
	return from(s->id());
}

QString DigiDocSignature::lastError() const { return m_lastError; }

QString DigiDocSignature::location() const
{
	QStringList l = locations();
	l.removeAll(QString());
	return l.join(QStringLiteral(", "));
}

QStringList DigiDocSignature::locations() const
{
	return {
		from( s->city() ).trimmed(),
		from( s->stateOrProvince() ).trimmed(),
		from( s->postalCode() ).trimmed(),
		from( s->countryName() ).trimmed()};
}

QByteArray DigiDocSignature::messageImprint() const
{
	return fromVector(s->messageImprint());
}

QSslCertificate DigiDocSignature::ocspCert() const
{
	return QSslCertificate(
		fromVector(s->OCSPCertificate()), QSsl::Der );
}

QDateTime DigiDocSignature::ocspTime() const
{
	return toTime(s->OCSPProducedAt());
}

const DigiDoc* DigiDocSignature::parent() const { return m_parent; }

void DigiDocSignature::parseException( DigiDocSignature::SignatureStatus &result, const digidoc::Exception &e ) const
{
	for(const Exception &child: e.causes())
	{
		switch( child.code() )
		{
		case Exception::ReferenceDigestWeak:
		case Exception::SignatureDigestWeak:
			m_warning |= DigestWeak;
			result = std::max( result, Warning );
			break;
		case Exception::DataFileNameSpaceWarning:
		case Exception::IssuerNameSpaceWarning:
		case Exception::ProducedATLateWarning:
		case Exception::MimeTypeWarning:
			result = std::max( result, Warning );
			break;
		case Exception::CertificateIssuerMissing:
		case Exception::CertificateUnknown:
		case Exception::OCSPResponderMissing:
		case Exception::OCSPCertMissing:
			result = std::max( result, Unknown );
			break;
		default:
			result = std::max( result, Invalid );
		}
		parseException( result, child );
	}
}

QString DigiDocSignature::policy() const
{
	return from(s->policy());
}

QString DigiDocSignature::profile() const
{
	return from(s->profile());
}

QString DigiDocSignature::role() const
{
	QStringList r = roles();
	r.removeAll(QString());
	return r.join(QStringLiteral(" / "));
}

QStringList DigiDocSignature::roles() const
{
	QStringList list;
	for(const std::string &role: s->signerRoles())
		list << from( role ).trimmed();
	return list;
}

void DigiDocSignature::setLastError( const Exception &e ) const
{
	QStringList causes;
	Exception::ExceptionCode code = Exception::General;
	DigiDoc::parseException(e, causes, code);
	m_lastError = causes.join('\n');
}

QString DigiDocSignature::signatureMethod() const
{ return from( s->signatureMethod() ); }

QString DigiDocSignature::signedBy() const
{
	return from(s->signedBy());
}

QDateTime DigiDocSignature::signTime() const
{
	return toTime(s->claimedSigningTime());
}

QString DigiDocSignature::spuri() const
{
	return from(s->SPUri());
}

QDateTime DigiDocSignature::toTime(const std::string &time) const
{
	QDateTime date;
	if(time.empty())
		return date;
	date = QDateTime::fromString(from(time), QStringLiteral("yyyy-MM-dd'T'hh:mm:ss'Z'"));
	date.setTimeSpec(Qt::UTC);
	return date;
}

QSslCertificate DigiDocSignature::tsCert() const
{
	return QSslCertificate(
		fromVector(s->TimeStampCertificate()), QSsl::Der );
}

QDateTime DigiDocSignature::tsTime() const
{
	return toTime(s->TimeStampTime());
}

QSslCertificate DigiDocSignature::tsaCert() const
{
	return QSslCertificate(
		fromVector(s->ArchiveTimeStampCertificate()), QSsl::Der );
}

QDateTime DigiDocSignature::tsaTime() const
{
	return toTime(s->ArchiveTimeStampTime());
}

DigiDocSignature::SignatureStatus DigiDocSignature::validate() const
{
	DigiDocSignature::SignatureStatus result = Valid;
	m_warning = 0;
	try
	{
		qApp->waitForTSL( m_parent->fileName() );
		s->validate();
	}
	catch( const Exception &e )
	{
		parseException( result, e );
		setLastError( e );
	}
	switch( result )
	{
	case Unknown:
		if ( validate(digidoc::Signature::POLv1) == Valid )
			return NonQSCD;
		return result;
	case Invalid: return result;
	default:
		if( SslCertificate( cert() ).type() & SslCertificate::TestType ||
			SslCertificate( ocspCert() ).type() & SslCertificate::TestType )
			return Test;

		return result;
	}
}

DigiDocSignature::SignatureStatus DigiDocSignature::validate(const std::string &policy) const
{
	DigiDocSignature::SignatureStatus result = Valid;
	try
	{
		s->validate(policy);
	}
	catch( const Exception &e )
	{
		parseException( result, e );
	}

	return result;
}

int DigiDocSignature::warning() const
{
	return int(m_warning);
}



SDocumentModel::SDocumentModel(DigiDoc *container)
: DocumentModel(container)
, doc(container)
{
	
}

bool SDocumentModel::addFile(const QString &file, const QString &mime)
{
	QFileInfo info(file);
	if(info.size() == 0)
	{
		WarningDialog dlg(tr("File you want to add is empty. Do you want to continue?"), qApp->activeWindow());
		dlg.setCancelText(tr("NO"));
		dlg.addButton(tr("YES"), 1);
		if(dlg.exec() != 1)
			return false;
	}
	QString fileName(info.fileName());
	for(int row = 0; row < rowCount(); row++)
	{
		if(fileName == from(doc->b->dataFiles().at(size_t(row))->fileName()))
		{
			qApp->showWarning(DocumentModel::tr("Cannot add the file to the envelope. File '%1' is already in container.").arg(fileName), QString());
			return false;
		}
	}
	if(doc->addFile(file, mime))
	{
		emit added(file);
		return true;
	}
	return false;
}

void SDocumentModel::addTempReference(const QString &file)
{
	doc->m_tempFiles << file;
}

QString SDocumentModel::data(int row) const
{
	if(row >= rowCount())
		return QString();

	return from(doc->b->dataFiles().at(size_t(row))->fileName());
}

QString SDocumentModel::fileSize(int row) const
{
	if(row >= rowCount())
		return QString();

	return FileDialog::fileSize(doc->b->dataFiles().at(size_t(row))->fileSize());
}

QString SDocumentModel::mime(int row) const
{
	if(row >= rowCount())
		return QString();

	return from(doc->b->dataFiles().at(size_t(row))->mediaType());
}

void SDocumentModel::open(int row)
{
	if(row >= rowCount())
		return;

	QFileInfo f(save(row, FileDialog::tempPath(
		FileDialog::safeName(from(doc->b->dataFiles().at(size_t(row))->fileName()))
	)));
	if( !f.exists() )
		return;
	doc->m_tempFiles << f.absoluteFilePath();
#if defined(Q_OS_WIN)
	QStringList exts = QProcessEnvironment::systemEnvironment().value( "PATHEXT" ).split( ';' );
	exts << ".PIF" << ".SCR";
	WarningDialog dlg(DocumentModel::tr("This is an executable file! "
		"Executable files may contain viruses or other malicious code that could harm your computer. "
		"Are you sure you want to launch this file?"), qApp->activeWindow());
	dlg.setCancelText(tr("NO"));
	dlg.addButton(tr("YES"), 1);
	if(exts.contains( "." + f.suffix(), Qt::CaseInsensitive ) && dlg.exec() != 1)
		return;
#else
	QFile::setPermissions( f.absoluteFilePath(), QFile::Permissions(0x6000) );
#endif
	emit openFile(f.absoluteFilePath());
}

bool SDocumentModel::removeRows(int row, int count)
{
	if(!doc->b)
		return false;

	try
	{
		for(int i = row + count - 1; i >= row; --i)
		{
			doc->b->removeDataFile(i);
			doc->modified = true;
			emit removed(i);
		}
		return true;
	}
	catch( const Exception &e ) { doc->setLastError( tr("Failed remove document from container"), e ); }
	return false;
}

int SDocumentModel::rowCount() const
{
	return int(doc->b->dataFiles().size());
}

QString SDocumentModel::save(int row, const QString &path) const
{
	if(row >= rowCount())
		return QString();

	QFile::remove( path );
	doc->b->dataFiles().at(size_t(row))->saveAs(path.toStdString());
	return path;
}



DigiDoc::DigiDoc(QObject *parent)
: QObject(parent)
, containerState(ContainerState::UnsignedContainer)
{
	m_documentModel.reset(new SDocumentModel(this));
}

DigiDoc::~DigiDoc() { clear(); }

bool DigiDoc::addFile(const QString &file, const QString &mime)
{
	if(!checkDoc(!b->signatures().empty(), tr("Cannot add files to signed container")))
		return false;
	try {
		b->addDataFile( to(file), to(mime));
		modified = true;
		return true;
	}
	catch( const Exception &e ) { setLastError( tr("Failed add file to container"), e ); }
	return false;
}

bool DigiDoc::checkDoc( bool status, const QString &msg ) const
{
	if( isNull() )
		qApp->showWarning( tr("Container is not open") );
	else if( status )
		qApp->showWarning( msg );
	return !isNull() && !status;
}

void DigiDoc::clear()
{
	b.reset();
	parentContainer.reset();
	m_fileName.clear();
	for(const QString &file: m_tempFiles)
		QFile::remove(file);
	m_tempFiles.clear();
	modified = false;
}

void DigiDoc::create( const QString &file )
{
	clear();
	b.reset(Container::create(to(file)));
	m_fileName = file;
	modified = false;
}

DocumentModel* DigiDoc::documentModel() const
{
	return m_documentModel.get();
}

QString DigiDoc::fileName() const { return m_fileName; }
bool DigiDoc::isService() const
{
	return b->mediaType() == "application/pdf";
}
bool DigiDoc::isModified() const { return modified; }
bool DigiDoc::isNull() const { return b == nullptr; }
bool DigiDoc::isReadOnlyTS() const
{
	return b->mediaType() == "application/vnd.etsi.asic-s+zip";
}
bool DigiDoc::isSupported() const
{
	return b->mediaType() == "application/vnd.etsi.asic-e+zip";
}

QString DigiDoc::mediaType() const
{ return b ? from( b->mediaType() ) : QString(); }

bool DigiDoc::move(const QString &to)
{
	bool success = false;
	if(containerState == ContainerState::UnsignedContainer)
	{
		success = true;
	}
	else
	{
		QFile f(m_fileName);
		if(!modified)
			success = f.rename(to);
		else if(save(to))
			success = f.remove();
	}

	if(success)
		m_fileName = to;

	return success;
}

QString DigiDoc::newSignatureID() const
{
	QStringList list;
	for(const Signature *s: b->signatures())
		list << QString::fromUtf8(s->id().c_str());
	unsigned int id = 0;
	while(list.contains(QStringLiteral("S%1").arg(id), Qt::CaseInsensitive)) ++id;
	return QStringLiteral("S%1").arg(id);
}

bool DigiDoc::open( const QString &file )
{
	qApp->waitForTSL( file );
	clear();
	if(QFileInfo(file).suffix().toLower() == QStringLiteral("pdf"))
	{
		QWidget *parent = qobject_cast<QWidget *>(QObject::parent());
		if(parent == nullptr)
			parent = qApp->activeWindow();
		WarningDialog dlg(tr("The verification of digital signatures in PDF format is performed through an external service. "
				"The file requiring verification will be forwarded to the service.\n"
				"The Information System Authority does not retain information regarding the files and users of the service."), parent);
		dlg.setCancelText(tr("CANCEL"));
		dlg.addButton(tr("OK"), ContainerSave);
		if(dlg.exec() != ContainerSave)
			return false;
	}
	try
	{
		b.reset(Container::open(to(file)));
		if(isReadOnlyTS())
		{
			const DataFile *f = b->dataFiles().at(0);
			if(QFileInfo(from(f->fileName())).suffix().toLower() == QStringLiteral("ddoc"))
			{
				const QString tmppath = FileDialog::tempPath(FileDialog::safeName(from(f->fileName())));
				f->saveAs(to(tmppath));
				QFileInfo f(tmppath);
				if(f.exists())
				{
					m_tempFiles << tmppath;
					parentContainer = std::move(b);
					b.reset(Container::open(to(tmppath)));
				}
			}
		}
		m_fileName = file;
		qApp->addRecent( file );
		modified = false;
		containerState = signatures().isEmpty() ? ContainerState::UnsignedSavedContainer : ContainerState::SignedContainer;
		return true;
	}
	catch( const Exception &e )
	{ setLastError( tr("An error occurred while opening the document."), e ); }
	return false;
}

bool DigiDoc::parseException(const Exception &e, QStringList &causes, Exception::ExceptionCode &code)
{
	causes << QStringLiteral("%1:%2 %3").arg(QFileInfo(from(e.file())).fileName()).arg(e.line()).arg(from(e.msg()));
	switch( e.code() )
	{
	case Exception::CertificateRevoked:
	case Exception::CertificateUnknown:
	case Exception::OCSPTimeSlot:
	case Exception::OCSPRequestUnauthorized:
	case Exception::TSTooManyRequests:
	case Exception::PINCanceled:
	case Exception::PINFailed:
	case Exception::PINIncorrect:
	case Exception::PINLocked:
		code = e.code();
	default: break;
	}
	for(const Exception &c: e.causes())
		if(!parseException(c, causes, code))
			return false;
	return true;
}

void DigiDoc::removeSignature( unsigned int num )
{
	if( !checkDoc( num >= b->signatures().size(), tr("Missing signature") ) )
		return;
	try { 
		b->removeSignature( num );
		modified = true;
	}
	catch( const Exception &e ) { setLastError( tr("Failed remove signature from container"), e ); }
}

bool DigiDoc::save( const QString &filename )
{
	if(!filename.isEmpty())
		m_fileName = filename;
	if(!saveAs(m_fileName))
		return false;
	qApp->addRecent(m_fileName);
	modified = false;
	containerState = signatures().isEmpty() ? ContainerState::UnsignedSavedContainer : ContainerState::SignedContainer;
	return true;
}

bool DigiDoc::saveAs(const QString &filename)
{
	OpEmitter op(this, Saving);
	try
	{
		b->save(to(filename));
		return true;
	}
	catch( const Exception &e ) { setLastError( tr("Failed to save container"), e ); }
	return false;
}

void DigiDoc::setLastError( const QString &msg, const Exception &e )
{
	QStringList causes;
	Exception::ExceptionCode code = Exception::General;
	parseException(e, causes, code);
	switch( code )
	{
	case Exception::CertificateRevoked:
		qApp->showWarning(tr("Certificate status revoked"), causes.join('\n')); break;
	case Exception::CertificateUnknown:
		qApp->showWarning(tr("Certificate status unknown"), causes.join('\n')); break;
	case Exception::OCSPTimeSlot:
		qApp->showWarning(tr("Please check your computer time. <a href='https://id.ee/index.php?id=39513'>Additional information</a>"), causes.join('\n')); break;
	case Exception::OCSPRequestUnauthorized:
		qApp->showWarning(tr("You have not granted IP-based access. "
			"Check the settings of your server access certificate."), causes.join('\n')); break;
	case Exception::TSTooManyRequests:
		qApp->showWarning(tr("The limit for digital signatures per month has been reached for this IP address. "
			"<a href=\"https://www.id.ee/index.php?id=39023\">Additional information</a>"), causes.join('\n')); break;
	case Exception::PINCanceled:
		break;
	case Exception::PINFailed:
		qApp->showWarning(tr("PIN Login failed"), causes.join('\n')); break;
	case Exception::PINIncorrect:
		qApp->showWarning(tr("PIN Incorrect"), causes.join('\n')); break;
	case Exception::PINLocked:
		qApp->showWarning(tr("PIN Locked. Unblock to reuse PIN."), causes.join('\n')); break;
	default:
		qApp->showWarning(msg, causes.join('\n')); break;
	}
}

bool DigiDoc::sign(const QString &city, const QString &state, const QString &zip,
	const QString &country, const QString &role, Signer *signer)
{
	if(!checkDoc(b->dataFiles().empty(), tr("Cannot add signature to empty container")))
		return false;

	OpEmitter op(this, Signing);
	try
	{
		signer->setSignatureProductionPlace(to(city), to(state), to(zip), to(country));
		std::vector<std::string> roles;
		if(!role.isEmpty())
			roles.push_back(to(role));
		signer->setSignerRoles(roles);
		signer->setProfile("time-stamp");
		qApp->waitForTSL( fileName() );
		b->sign(signer);
		modified = true;
		return true;
	}
	catch( const Exception &e )
	{
		QStringList causes;
		Exception::ExceptionCode code = Exception::General;
		parseException(e, causes, code);
		if( code == Exception::PINIncorrect )
		{
			qApp->showWarning( tr("PIN Incorrect") );
			if( !(qApp->signer()->tokensign().flags() & TokenData::PinLocked) )
				return sign(city, state, zip, country, role, signer);
		}
		else
			setLastError( tr("Failed to sign container"), e );
	}
	return false;
}

QList<DigiDocSignature> DigiDoc::signatures() const
{
	QList<DigiDocSignature> list;
	if( isNull() )
		return list;
	for(const Signature *signature: b->signatures())
		list << DigiDocSignature(signature, this);
	return list;
}

ContainerState DigiDoc::state()
{
	return containerState;
}

QList<DigiDocSignature> DigiDoc::timestamps() const
{
	QList<DigiDocSignature> list;
	if(!parentContainer)
		return list;
	for(const Signature *signature: parentContainer->signatures())
		list << DigiDocSignature(signature, this);
	return list;
}

DigiDoc::DocumentType DigiDoc::documentType() const
{
	return checkDoc() && b->mediaType() == "application/vnd.etsi.asic-e+zip" ? BDoc2Type : DDocType;
}
