// SPDX-FileCopyrightText: Estonian Information System Authority
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "QCryptoBackend.h"

#include <QtCore/QObject>
#include <QtCore/QIODevice>
#include <QtCore/QFile>

#include <cdoc/Configuration.h>
#include <cdoc/CryptoBackend.h>
#include <cdoc/Logger.h>
#include <cdoc/NetworkBackend.h>
#include <cdoc/Io.h>
#include <cdoc/Recipient.h>

//
// libcdoc handler implementations
//

//
// Configuration
//
// Provides KEYSERVER_SEND_URL and KEYSERVER_FETCH_URL
//

struct DDConfiguration : public libcdoc::Configuration {
	std::string getValue(std::string_view domain,
						 std::string_view param) const final;

	explicit DDConfiguration() = default;
};

//
// CryptoBackend
//
// Bridges to qApp->cryptoManager()
//

struct DDCryptoBackend final : public libcdoc::CryptoBackend {
	static constexpr int BACKEND_ERROR = -503;
	static constexpr int PIN_CANCELED = -504;
	static constexpr int PIN_INCORRECT = -505;
	static constexpr int PIN_LOCKED = -506;
	static constexpr int IN_PROGRESS = -507;
	libcdoc::result_t decryptRSA(std::vector<uint8_t> &result,
								 const std::vector<uint8_t> &data, bool oaep,
								 unsigned int idx) final;
	libcdoc::result_t deriveConcatKDF(std::vector<uint8_t> &dst,
									  const std::vector<uint8_t> &publicKey,
									  const std::string &digest,
									  const std::vector<uint8_t> &algorithmID,
									  const std::vector<uint8_t> &partyUInfo,
									  const std::vector<uint8_t> &partyVInfo,
									  unsigned int idx) final;
	libcdoc::result_t deriveHMACExtract(std::vector<uint8_t> &dst,
										const std::vector<uint8_t> &publicKey,
										const std::vector<uint8_t> &salt,
										unsigned int idx) final;
	libcdoc::result_t extractHKDF(std::vector<uint8_t> &dst,
								 const std::vector<uint8_t> &salt,
								 const std::vector<uint8_t> &pwSalt,
								 int32_t kdfIter,
								 unsigned int idx) final;
	std::string getLastErrorStr(libcdoc::result_t code) const final;

	std::unique_ptr<QCryptoBackend> backend;
	QByteArray secret;
	libcdoc::result_t backend_status = libcdoc::OK;

	explicit DDCryptoBackend() = default;

	void setBackend(std::unique_ptr<QCryptoBackend> &&backend) {
		this->backend = std::move(backend);
		backend_status = libcdoc::OK;
	}
	void setSecret(QByteArray &&value) {
		clearSecret();
		secret = std::move(value);
	}
	void clearSecret() {
		secret.fill(0);
		secret.clear();
	}
};

//
// NetworkBackend
//
// Supplies libcdoc's HTTP client with the key server certificates, the
// authentication key and the proxy settings
//

struct DDNetworkBackend final : public libcdoc::NetworkBackend, private QObject {
	static constexpr int BACKEND_ERROR = -303;

	std::string getLastErrorStr(libcdoc::result_t code) const final;
	libcdoc::result_t sendKey(libcdoc::NetworkBackend::CapsuleInfo &dst,
							  const std::string &url,
							  const std::vector<uint8_t> &rcpt_key,
							  const std::vector<uint8_t> &key_material,
							  const std::string &type,
							  uint64_t expiry_ts) final;
	libcdoc::result_t
	fetchKey(std::vector<uint8_t> &result, const std::string &keyserver_id,
			 const std::string &transaction_id) final;

	libcdoc::result_t getClientTLSCertificate(std::vector<uint8_t> &dst) final;
	libcdoc::result_t getPeerTLSCertificates(std::vector<std::vector<uint8_t>> &dst) final;
	libcdoc::result_t getProxyCredentials(ProxyCredentials &credentials) const final;
	libcdoc::result_t signTLS(std::vector<uint8_t> &dst, libcdoc::CryptoBackend::HashAlgorithm algorithm,
		const std::vector<uint8_t> &digest) final;

	explicit DDNetworkBackend(DDCryptoBackend &_crypto) : crypto(_crypto) {}

	DDCryptoBackend &crypto;
	std::string last_error;

private:
	bool sending = false;
	mutable std::string proxyPassword;
};

//
// ILogger
//
// Bridges to Qt logging system
//

class DDCDocLogger final : private libcdoc::Logger {
  public:
	static void setUpLogger(const QString &path);
	static void setLogLevel(libcdoc::LogLevel level);

  private:
	DDCDocLogger() = default;
	static DDCDocLogger *getLogger();
	void logMessage(libcdoc::LogLevel level, std::string_view file, int line, std::string_view message) final;
	QFile ofs;
};

class CDocSupport {
public:
	static std::vector<libcdoc::FileInfo> getCDocFileList(const QString &filename);
};

//
// DataSource and consumer that can share temporary files/buffwers
//

struct IOEntry
{
	std::string name, mime;
	int64_t size;
	std::unique_ptr<QIODevice> data;
};

struct TempListConsumer final : public libcdoc::MultiDataConsumer {
	/// Why extraction stopped, so policy limits can be distinguished from
	/// malformed container data.
	enum class Rejection : quint8 { None, Count, Disk, Overrun };

	TempListConsumer();
	~TempListConsumer();

	libcdoc::result_t write(const uint8_t *src, size_t size) noexcept final;
	libcdoc::result_t close() noexcept final;
	bool isError() noexcept final;
	libcdoc::result_t open(const std::string &name,
						   int64_t size) final;

	Rejection rejection() const noexcept { return _rejection; }

	std::vector<IOEntry> files;

private:
	static constexpr size_t MAX_MEMORY_SIZE = 500ULL * 1024ULL * 1024ULL;
	static constexpr size_t MAX_DISK_SIZE = 8ULL * 1024ULL * 1024ULL * 1024ULL;
	static constexpr size_t MIN_FREE_DISK_SIZE = 1ULL * 1024ULL * 1024ULL * 1024ULL;
	static constexpr size_t MAX_FILE_COUNT = 1000;

	[[nodiscard]] bool exceedsDiskBudget(size_t size) const noexcept
	{
		return _disk_used > _disk_limit || size > _disk_limit - _disk_used;
	}
	libcdoc::result_t reject(Rejection reason, libcdoc::result_t code) noexcept;

	Rejection _rejection = Rejection::None;
	size_t _memory_used = 0;
	size_t _disk_used = 0;
	size_t _disk_limit = MAX_DISK_SIZE;
	/// Size the container declared for the open entry, or -1 if unstated.
	int64_t _declared = -1;
	/// Whether the open entry is buffered in memory rather than on disk.
	bool _in_memory = false;
};

struct StreamListSource final : public libcdoc::MultiDataSource {
	StreamListSource(const std::vector<IOEntry> &files);

	libcdoc::result_t read(uint8_t *dst, size_t size) noexcept final;
	bool isError() noexcept final;
	bool isEof() noexcept final;
	libcdoc::result_t getNumComponents() final;
	libcdoc::result_t next(std::string &name, int64_t &size) final;

	const std::vector<IOEntry> &_files;
	int64_t _current = -1;
};
