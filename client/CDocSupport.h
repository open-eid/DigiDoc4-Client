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

#include "QCryptoBackend.h"

#include <QtCore/QObject>
#include <QtCore/QIODevice>
#include <QtCore/QFile>
#include <QtCore/QUrl>

#include <cdoc/Configuration.h>
#include <cdoc/CryptoBackend.h>
#include <cdoc/Logger.h>
#include <cdoc/NetworkBackend.h>
#include <cdoc/Io.h>
#include <cdoc/Recipient.h>

#include <atomic>
#include <functional>

//
// libcdoc handler implementations
//

//
// Configuration
//
// Provides KEYSERVER_SEND_URL and KEYSERVER_FETCH_URL, and the Mobile-ID / Smart-ID
// AUTH_SERVER, RP_SERVER and SHARE_SERVER_URLS from the CDOC2-CONF entries
//

struct DDConfiguration : public libcdoc::Configuration {
	std::string getValue(std::string_view domain,
						 std::string_view param) const final;
	// Whether the authentication and relying party servers for a lock's share
	// servers (Lock::SHARE_URLS) are configured, i.e. Mobile-ID / Smart-ID can open it
	static bool shareServersKnown(const std::string &shareUrls);
	// Mobile-ID / Smart-ID decryption: Configuration::SHARE_SIGNER_MID or
	// SHARE_SIGNER_SID, the Mobile-ID phone number, and the lock's share servers
	// (Lock::SHARE_URLS) that select the CDOC2-CONF entry
	std::string shareSigner;
	std::string phone;
	std::string shareUrls;

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
	libcdoc::result_t getSecret(std::vector<uint8_t> &secret,
								unsigned int idx) final;
	std::string getLastErrorStr(libcdoc::result_t code) const final;

	std::unique_ptr<QCryptoBackend> backend;
	std::vector<uint8_t> secret;

	explicit DDCryptoBackend() = default;

	void setBackend(std::unique_ptr<QCryptoBackend> &&backend) {
		this->backend = std::move(backend);
	}
};

//
// NetworkBackend
//
// Bridges to QNetworkAccessManager
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
	// Key share and Mobile-ID / Smart-ID requests, over Qt for the proxy and TLS setup
	libcdoc::result_t get(const std::string &url, std::vector<uint8_t> &body,
		std::map<std::string, std::string> &headers, bool client_cert) final;
	libcdoc::result_t post(const std::string &url, std::vector<uint8_t> &body,
		std::map<std::string, std::string> &headers, bool client_cert) final;
	libcdoc::result_t getAuthResponse(const std::string &url, std::vector<uint8_t> &body,
		std::map<std::string, std::string> &headers) final;
	libcdoc::result_t getSignResponse(const std::string &url, const std::string &post_path,
		std::vector<uint8_t> &body, std::map<std::string, std::string> &headers,
		const std::string &poll_path_prefix) final;
	libcdoc::result_t showFeedback(SIDMIDFeedback &feedback) final;

	libcdoc::result_t
	getClientTLSCertificate(std::vector<uint8_t> &dst) final {
		return libcdoc::NOT_IMPLEMENTED;
	}
	libcdoc::result_t getPeerTLSCertificates(
		std::vector<std::vector<uint8_t>> &dst) final {
		return libcdoc::NOT_IMPLEMENTED;
	}

	explicit DDNetworkBackend(DDCryptoBackend &_crypto) : crypto(_crypto) {}

	DDCryptoBackend &crypto;
	std::string last_error;
	// Gets each Mobile-ID / Smart-ID verification code, on the main thread
	std::function<void(int code)> onFeedback;
	// Set from the main thread to end a Mobile-ID / Smart-ID session
	std::atomic_bool canceled = false;

private:
	libcdoc::result_t request(bool post, const std::string &url, std::vector<uint8_t> &body,
		std::map<std::string, std::string> &headers, bool client_cert);
	libcdoc::result_t send(bool post, const QUrl &url, std::vector<uint8_t> &body,
		std::map<std::string, std::string> &headers, int timeout = 30'000);
	libcdoc::result_t poll(const QUrl &url, QLatin1String field, std::vector<uint8_t> &body,
		std::map<std::string, std::string> &headers);
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
	static constexpr int64_t MAX_VEC_SIZE = 500L * 1024L * 1024L;

	explicit TempListConsumer(size_t max_memory_size = 500L * 1024L * 1024L)
		: _max_memory_size(max_memory_size) {}
	~TempListConsumer();

	libcdoc::result_t write(const uint8_t *src, size_t size) noexcept final;
	libcdoc::result_t close() noexcept final;
	bool isError() noexcept final;
	libcdoc::result_t open(const std::string &name,
						   int64_t size) final;

	size_t _max_memory_size;
	std::vector<IOEntry> files;
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
