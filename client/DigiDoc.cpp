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
#include "CheckConnection.h"
#include "QSigner.h"
#include "SslCertificate.h"
#include "TokenData.h"
#include "Utils.h"
#include "dialogs/FileDialog.h"
#include "dialogs/WarningDialog.h"

#include <digidocpp/DataFile.h>
#include <digidocpp/Signature.h>
#include <digidocpp/crypto/X509Cert.h>

#include <QtCore/QDateTime>
#include <QtCore/QFileInfo>
#include <QtCore/QStringList>
#include <QtCore/QUrl>
#include <QtGui/QDesktopServices>

#include <algorithm>

#if defined(Q_OS_WIN)
#include <qt_windows.h>
#include <fileapi.h>
#endif

using namespace digidoc;
using namespace ria::qdigidoc4;

static std::string to(const QString &str) { return str.toStdString(); }
static QString from(const std::string &str) { return FileDialog::normalized(QString::fromStdString(str)); }



DigiDocSignature::DigiDocSignature(const digidoc::Signature *signature, const DigiDoc *parent, bool isTimeStamped)
	: s(signature)
	, m_parent(parent)
	, m_isTimeStamped(isTimeStamped)
	, m_status(validate())
{}

QSslCertificate DigiDocSignature::cert() const
{
	return toCertificate(s->signingCertificate());
}

QDateTime DigiDocSignature::claimedTime() const
{
	return toTime(s->claimedSigningTime());
}

bool DigiDocSignature::isInvalid() const
{
	return m_status >= Invalid;
}

QString DigiDocSignature::lastError() const { return m_lastError; }

QString DigiDocSignature::location() const
{
	QStringList l = locations();
	l.removeAll({});
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
	std::vector<unsigned char> d = s->messageImprint();
	return {(const char *)d.data(), int(d.size())};
}

QSslCertificate DigiDocSignature::ocspCert() const
{
	return toCertificate(s->OCSPCertificate());
}

QDateTime DigiDocSignature::ocspTime() const
{
	return toTime(s->OCSPProducedAt());
}

const DigiDoc* DigiDocSignature::parent() const { return m_parent; }

void DigiDocSignature::parseException(DigiDocSignature::SignatureStatus &result, const digidoc::Exception &e)
{
	for(const Exception &child: e.causes())
	{
		switch( child.code() )
		{
		case Exception::ReferenceDigestWeak:
		case Exception::SignatureDigestWeak:
			if(!m_isTimeStamped)
			{
				m_warning |= DigestWeak;
				result = std::max( result, Warning );
			}
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
	r.removeAll({});
	return r.join(QStringLiteral(" / "));
}

QStringList DigiDocSignature::roles() const
{
	QStringList list;
	for(const std::string &role: s->signerRoles())
		list.append(from( role ).trimmed());
	return list;
}

void DigiDocSignature::setLastError(const Exception &e)
{
	Exception::ExceptionCode code = Exception::General;
	QStringList causes = DigiDoc::parseException(e, code);
	m_lastError = code == Exception::OCSPBeforeTimeStamp ?
		DigiDoc::tr("The timestamp added to the signature must be taken before validity confirmation.") :
		causes.join('\n');
}

QString DigiDocSignature::signatureMethod() const
{ return from( s->signatureMethod() ); }

QString DigiDocSignature::signedBy() const
{
	return from(s->signedBy());
}

QString DigiDocSignature::spuri() const
{
	return from(s->SPUri());
}

DigiDocSignature::SignatureStatus DigiDocSignature::status() const
{
	return m_status;
}

QSslCertificate DigiDocSignature::toCertificate(const std::vector<unsigned char> &der)
{
	return QSslCertificate(QByteArray::fromRawData((const char *)der.data(), int(der.size())), QSsl::Der);
}

QDateTime DigiDocSignature::toTime(const std::string &time)
{
	if(time.empty())
		return {};
	QDateTime date = QDateTime::fromString(from(time), QStringLiteral("yyyy-MM-dd'T'hh:mm:ss'Z'"));
	date.setTimeSpec(Qt::UTC);
	return date;
}

QDateTime DigiDocSignature::trustedTime() const
{
	return toTime(s->trustedSigningTime());
}

QSslCertificate DigiDocSignature::tsCert() const
{
	return toCertificate(s->TimeStampCertificate());
}

QDateTime DigiDocSignature::tsTime() const
{
	return toTime(s->TimeStampTime());
}

QSslCertificate DigiDocSignature::tsaCert() const
{
	return toCertificate(s->ArchiveTimeStampCertificate());
}

QDateTime DigiDocSignature::tsaTime() const
{
	return toTime(s->ArchiveTimeStampTime());
}

DigiDocSignature::SignatureStatus DigiDocSignature::validate()
{
	if(!s)
		return Invalid;
	DigiDocSignature::SignatureStatus result = Valid;
	try
	{
		s->validate();
		return Valid;
	}
	catch(const Exception &e)
	{
		parseException(result, e);
		setLastError(e);
	}
	if(result != Unknown)
		return result;
	try
	{
		s->validate(digidoc::Signature::POLv1);
		return NonQSCD;
	}
	catch(const Exception &e)
	{
		parseException(result, e);
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
{}

bool SDocumentModel::addFile(const QString &file, const QString &mime)
{
	QFileInfo info(file);
	if(info.size() == 0)
	{
		WarningDialog::show(qApp->mainWindow(), DocumentModel::tr("Cannot add empty file to the container."));
		return false;
	}
	QString fileName(info.fileName());
	if(fileName == QStringLiteral("mimetype"))
	{
		WarningDialog::show(qApp->mainWindow(), DocumentModel::tr("Cannot add file with name 'mimetype' to the envelope."));
		return false;
	}
	for(int row = 0; row < rowCount(); row++)
	{
		if(fileName == from(doc->b->dataFiles().at(size_t(row))->fileName()))
		{
			WarningDialog::show(qApp->mainWindow(), DocumentModel::tr("Cannot add the file to the envelope. File '%1' is already in container.")
				.arg(FileDialog::normalized(fileName)));
			return false;
		}
	}
	if(doc->addFile(file, mime))
	{
		emit added(FileDialog::normalized(fileName));
		return true;
	}
	return false;
}

void SDocumentModel::addTempReference(const QString &file)
{
	doc->m_tempFiles.append(file);
}

QString SDocumentModel::data(int row) const
{
	if(row >= rowCount())
		return {};

	return from(doc->b->dataFiles().at(size_t(row))->fileName());
}

QString SDocumentModel::fileSize(int row) const
{
	if(row >= rowCount())
		return {};

	return FileDialog::fileSize(doc->b->dataFiles().at(size_t(row))->fileSize());
}

QString SDocumentModel::mime(int row) const
{
	if(row >= rowCount())
		return {};

	return from(doc->b->dataFiles().at(size_t(row))->mediaType());
}

void SDocumentModel::open(int row)
{
	if(row >= rowCount())
		return;

	QString path = FileDialog::tempPath(FileDialog::safeName(from(doc->b->dataFiles().at(size_t(row))->fileName())));
	if(!verifyFile(path))
		return;
	QFileInfo f(save(row, path));
	if( !f.exists() )
		return;
	doc->m_tempFiles.append(f.absoluteFilePath());
#if defined(Q_OS_WIN)
	::SetFileAttributesW(f.absoluteFilePath().toStdWString().c_str(), FILE_ATTRIBUTE_READONLY);
#else
	QFile::setPermissions(f.absoluteFilePath(), QFile::Permissions(QFile::Permission::ReadOwner));
#endif
	if(!doc->fileName().endsWith(QStringLiteral(".pdf"), Qt::CaseInsensitive) && FileDialog::isSignedPDF(f.absoluteFilePath()))
		qApp->showClient({ f.absoluteFilePath() }, false, false, true);
	else
		QDesktopServices::openUrl(QUrl::fromLocalFile(f.absoluteFilePath()));
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
		return {};

	QFile::remove( path );
	int zone = FileDialog::fileZone(doc->fileName());
	doc->b->dataFiles().at(size_t(row))->saveAs(path.toStdString());
	FileDialog::setFileZone(path, zone);
	return path;
}



DigiDoc::DigiDoc(QObject *parent)
	: QObject(parent)
	, m_documentModel(new SDocumentModel(this))
{
}

DigiDoc::~DigiDoc() { clear(); }

bool DigiDoc::addFile(const QString &file, const QString &mime)
{
	if(isError(!b->signatures().empty(), tr("Cannot add files to signed container")))
		return false;
	try {
		b->addDataFile( to(file), to(mime));
		modified = true;
		return true;
	}
	catch( const Exception &e ) { setLastError( tr("Failed add file to container"), e ); }
	return false;
}

void DigiDoc::clear()
{
	b.reset();
	parentContainer.reset();
	m_signatures.clear();
	m_timestamps.clear();
	m_fileName.clear();
	for(const QString &file: m_tempFiles)
	{
#if defined(Q_OS_WIN)
		//reset read-only attribute to enable delete file
		::SetFileAttributesW(file.toStdWString().c_str(), FILE_ATTRIBUTE_NORMAL);
#endif
		QFile::remove(file);
	}

	m_tempFiles.clear();
	modified = false;
}

void DigiDoc::create( const QString &file )
{
	clear();
	b = Container::createPtr(to(file));
	m_fileName = file;
}

DocumentModel* DigiDoc::documentModel() const
{
	return m_documentModel.get();
}

QString DigiDoc::fileName() const { return m_fileName; }

bool DigiDoc::isError(bool failure, const QString &msg) const
{
	if(!b)
		qApp->showWarning(tr("Container is not open"));
	else if(failure)
		qApp->showWarning(msg);
	return !b || failure;
}

bool DigiDoc::isPDF() const
{
	return b && b->mediaType() == "application/pdf";
}
bool DigiDoc::isModified() const { return modified; }
bool DigiDoc::isSupported() const
{
	return b && b->mediaType() == "application/vnd.etsi.asic-e+zip";
}

QString DigiDoc::mediaType() const
{ return b ? from( b->mediaType() ) : QString(); }

bool DigiDoc::move(const QString &to)
{
	bool success = containerState == ContainerState::UnsignedContainer;
	if(!success)
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

bool DigiDoc::open( const QString &file )
{
	QWidget *parent = qobject_cast<QWidget *>(QObject::parent());
	if(parent == nullptr)
		parent = qApp->activeWindow();
	qApp->waitForTSL( file );
	clear();
	auto serviceConfirmation = [parent] {
		WarningDialog dlg(tr("Signed document in PDF and DDOC format will be transmitted to the Digital Signature Validation Service SiVa to verify the validity of the digital signature. "
			"Read more information about transmitted data to Digital Signature Validation service from <a href=\"https://www.id.ee/en/article/data-protection-conditions-for-the-id-software-of-the-national-information-system-authority/\">here</a>.<br />"
			"Do you want to continue?"), parent);
		dlg.setCancelText(tr("CANCEL"));
		dlg.addButton(tr("YES"), ContainerSave);
		return dlg.exec() == ContainerSave;
	};
	if((file.endsWith(QStringLiteral(".pdf"), Qt::CaseInsensitive) ||
		file.endsWith(QStringLiteral(".ddoc"), Qt::CaseInsensitive)) && !serviceConfirmation())
		return false;

	try {
		WaitDialogHolder waitDialog(parent, tr("Opening"), false);
		return waitFor([&] {
			b = Container::openPtr(to(file));
			if(b && b->mediaType() == "application/vnd.etsi.asic-s+zip" &&
				b->dataFiles().size() == 1 &&
				b->signatures().size() == 1)
			{
				const DataFile *f = b->dataFiles().at(0);
				if(from(f->fileName()).endsWith(QStringLiteral(".ddoc"), Qt::CaseInsensitive)  &&
					CheckConnection().check(QStringLiteral("https://id.eesti.ee/config.json")) &&
					dispatchToMain(serviceConfirmation))
				{
					const QString tmppath = FileDialog::tempPath(FileDialog::safeName(from(f->fileName())));
					f->saveAs(to(tmppath));
					if(QFileInfo::exists(tmppath))
					{
						m_tempFiles.append(tmppath);
						try {
							parentContainer = std::exchange(b, Container::openPtr(to(tmppath)));
						} catch(const Exception &) {}
					}
				}
			}
			bool isTimeStamped = parentContainer && parentContainer->signatures().at(0)->trustedSigningTime().compare("2018-07-01T00:00:00Z") < 0;
			for(const Signature *signature: b->signatures())
				m_signatures.append(DigiDocSignature(signature, this, isTimeStamped));
			if(parentContainer)
			{
				for(const Signature *signature: parentContainer->signatures())
					m_timestamps.append(DigiDocSignature(signature, this));
			}
			qApp->addRecent(file);
			m_fileName = file;
			containerState = signatures().isEmpty() ? ContainerState::UnsignedSavedContainer : ContainerState::SignedContainer;
			return true;
		});
	} catch(const Exception &e) {
		switch(e.code())
		{
		case Exception::NetworkError:
			setLastError(tr("Connecting to SiVa server failed! Please check your internet connection."), e);
			break;
		case Exception::HostNotFound:
		case Exception::InvalidUrl:
			setLastError(tr("Connecting to SiVa server failed! Please check your internet connection and network settings."), e);
			break;
		default:
			setLastError(tr("An error occurred while opening the document."), e);
			break;
		}
	}
	return false;
}

QStringList DigiDoc::parseException(const Exception &e, Exception::ExceptionCode &code)
{
	QStringList causes{ QStringLiteral("%1:%2 %3").arg(QFileInfo(from(e.file())).fileName()).arg(e.line()).arg(from(e.msg())) };
	switch( e.code() )
	{
	case Exception::CertificateRevoked:
	case Exception::CertificateUnknown:
	case Exception::OCSPTimeSlot:
	case Exception::OCSPRequestUnauthorized:
	case Exception::OCSPBeforeTimeStamp:
	case Exception::TSForbidden:
	case Exception::TSTooManyRequests:
	case Exception::PINCanceled:
	case Exception::PINFailed:
	case Exception::PINIncorrect:
	case Exception::PINLocked:
	case Exception::NetworkError:
	case Exception::HostNotFound:
	case Exception::InvalidUrl:
		code = e.code();
		break;
	default: break;
	}
	for(const Exception &c: e.causes())
		causes.append(parseException(c, code));
	return causes;
}

void DigiDoc::removeSignature( unsigned int num )
{
	if(isError(num >= b->signatures().size(), tr("Missing signature")))
		return;
	try {
		modified = waitFor([&] {
			b->removeSignature(num);
			m_signatures.removeAt(num);
			return true;
		});
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
	try
	{
		return waitFor([&] {
			parentContainer ? parentContainer->save(to(filename)) : b->save(to(filename));
			return true;
		});
	}
	catch( const Exception &e ) {
		QFile::remove(filename);
		if(!QFile::copy(m_fileName, filename))
			setLastError(tr("Failed to save container"), e);
	}
	return false;
}

void DigiDoc::setLastError( const QString &msg, const Exception &e )
{
	Exception::ExceptionCode code = Exception::General;
	QStringList causes = parseException(e, code);
	switch( code )
	{
	case Exception::CertificateRevoked:
		qApp->showWarning(tr("Certificate status revoked"), causes.join('\n')); break;
	case Exception::CertificateUnknown:
		qApp->showWarning(tr("Certificate status unknown"), causes.join('\n')); break;
	case Exception::OCSPTimeSlot:
		qApp->showWarning(tr("Please check your computer time. <a href='https://www.id.ee/en/article/digidoc4-client-error-please-check-your-computer-time-2/'>Additional information</a>"), causes.join('\n')); break;
	case Exception::OCSPRequestUnauthorized:
		qApp->showWarning(tr("You have not granted IP-based access. "
			"Check your validity confirmation service access settings."), causes.join('\n')); break;
	case Exception::TSForbidden:
		qApp->showWarning(tr("Failed to sign container. "
			"Check your Time-Stamping service access settings."), causes.join('\n')); break;
	case Exception::TSTooManyRequests:
		qApp->showWarning(tr("The limit for digital signatures per month has been reached for this IP address. "
			"<a href=\"https://www.id.ee/en/article/for-organisations-that-sign-large-quantities-of-documents-using-digidoc4-client/\">Additional information</a>"), causes.join('\n')); break;
	case Exception::PINCanceled:
		break;
	case Exception::PINFailed:
		qApp->showWarning(tr("PIN Login failed"), causes.join('\n')); break;
	case Exception::PINIncorrect:
		qApp->showWarning(tr("PIN Incorrect"), causes.join('\n')); break;
	case Exception::PINLocked:
		qApp->showWarning(tr("PIN Locked. Unblock to reuse PIN."), causes.join('\n')); break;
	case Exception::NetworkError: // use passed message for these thre exceptions
	case Exception::HostNotFound:
	case Exception::InvalidUrl:
	default:
		qApp->showWarning(msg, causes.join('\n')); break;
	}
}

bool DigiDoc::sign(const QString &city, const QString &state, const QString &zip,
	const QString &country, const QString &role, Signer *signer)
{
	if(isError(b->dataFiles().empty(), tr("Cannot add signature to empty container")))
		return false;

	try
	{
		signer->setSignatureProductionPlace(to(city), to(state), to(zip), to(country));
		std::vector<std::string> roles;
		if(!role.isEmpty())
			roles.push_back(to(role));
		signer->setSignerRoles(roles);
		signer->setProfile("time-stamp");
		qApp->waitForTSL( fileName() );
		digidoc::Signature *s = b->sign(signer);
		return modified = waitFor([&] {
			m_signatures.append(DigiDocSignature(s, this, false));
			return true;
		});
	}
	catch( const Exception &e )
	{
		Exception::ExceptionCode code = Exception::General;
		QStringList causes = parseException(e, code);
		switch(code)
		{
		case Exception::PINIncorrect:
			qApp->showWarning(tr("PIN Incorrect"));
			return sign(city, state, zip, country, role, signer);
		case Exception::NetworkError:
		case Exception::HostNotFound:
			qApp->showWarning(tr("Failed to sign container. Please check the access to signing services and network settings."), causes.join('\n')); break;
		case Exception::InvalidUrl:
			qApp->showWarning(tr("Failed to sign container. Signing service URL is incorrect."), causes.join('\n')); break;
		default:
			setLastError(tr("Failed to sign container."), e); break;
		}
	}
	return false;
}

QList<DigiDocSignature> DigiDoc::signatures() const
{
	return m_signatures;
}

ContainerState DigiDoc::state()
{
	return containerState;
}

QList<DigiDocSignature> DigiDoc::timestamps() const
{
	return m_timestamps;
}
