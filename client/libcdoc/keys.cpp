#define __KEYS_CPP__

#include "cdoc2.h"
#include "certificate.h"

#include "header_generated.h"

#include "keys.h"

namespace libcdoc {

#if 0
bool
EncKey::isTheSameRecipient(const EncKey& other) const
{
	if (!isPKI()) return false;
	if (!other.isPKI()) return false;
	const EncKeyPKI& pki = static_cast<const EncKeyPKI&>(*this);
	const EncKeyPKI& other_pki = static_cast<const EncKeyPKI&>(other);
	return pki.rcpt_key == other_pki.rcpt_key;
}

bool
EncKey::isTheSameRecipient(const std::vector<uint8_t>& public_key) const
{
	if (!isPKI()) return false;
	const EncKeyPKI& pki = static_cast<const EncKeyPKI&>(*this);
	if (pki.rcpt_key.empty() || public_key.empty()) return false;
	return pki.rcpt_key == public_key;
}

std::string
EncKeySymmetric::getSaltForExpand() const
{
	return CDoc2Reader::KEK + cdoc20::header::EnumNameFMKEncryptionMethod(cdoc20::header::FMKEncryptionMethod::XOR) + label;
}

EncKeyCert::EncKeyCert(const std::string& _label, const std::vector<uint8_t> &c)
	: EncKeyPKI(EncKey::Type::CERTIFICATE)
{
	label = _label;
	setCert(c);
}

void
EncKeyCert::setCert(const std::vector<uint8_t> &_cert)
{
	cert = _cert;
	Certificate ssl(_cert);
	std::vector<uint8_t> pkey = ssl.getPublicKey();
	Certificate::Algorithm algo = ssl.getAlgorithm();

	rcpt_key = pkey;
	pk_type = (algo == libcdoc::Certificate::RSA) ? PKType::RSA : PKType::ECC;
}
#endif

bool
CKey::isTheSameRecipient(const CKey& other) const
{
	if (!isPKI()) return false;
	if (!other.isPKI()) return false;
	const CKeyPKI& pki = static_cast<const CKeyPKI&>(*this);
	const CKeyPKI& other_pki = static_cast<const CKeyPKI&>(other);
	return pki.rcpt_key == other_pki.rcpt_key;
}

bool
CKey::isTheSameRecipient(const std::vector<uint8_t>& public_key) const
{
	if (!isPKI()) return false;
	const CKeyPKI& pki = static_cast<const CKeyPKI&>(*this);
	if (pki.rcpt_key.empty() || public_key.empty()) return false;
	return pki.rcpt_key == public_key;
}

std::string
CKeySymmetric::getSaltForExpand() const
{
	return CDoc2Reader::KEK + cdoc20::header::EnumNameFMKEncryptionMethod(cdoc20::header::FMKEncryptionMethod::XOR) + label;
}

CKeyCert::CKeyCert(Type _type, const std::string& _label, const std::vector<uint8_t> &c)
	: CKeyPKI(_type)
{
	label = _label;
	setCert(c);
}

void
CKeyCert::setCert(const std::vector<uint8_t> &_cert)
{
	cert = _cert;
	Certificate ssl(_cert);
	std::vector<uint8_t> pkey = ssl.getPublicKey();
	Certificate::Algorithm algo = ssl.getAlgorithm();

	rcpt_key = pkey;
	pk_type = (algo == libcdoc::Certificate::RSA) ? PKType::RSA : PKType::ECC;
}

std::shared_ptr<CKeyServer>
CKeyServer::fromKey(const std::vector<uint8_t> _key, PKType _pk_type) {
	return std::shared_ptr<CKeyServer>(new CKeyServer(_key, _pk_type));
}

} // namespace libcdoc
