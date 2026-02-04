#ifndef __CDOCSUPPORT_H__
#define __CDOCSUPPORT_H__

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

#include <QtCore/QObject>
#include <QtCore/QIODevice>

#include <cdoc/Configuration.h>
#include <cdoc/CryptoBackend.h>
#include <cdoc/ILogger.h>
#include <cdoc/NetworkBackend.h>
#include <cdoc/Io.h>

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
// Bridges to qApp->signer()
//

struct DDCryptoBackend : public libcdoc::CryptoBackend {
	static constexpr int BACKEND_ERROR = -303;
	static constexpr int PIN_CANCELED = -304;
	static constexpr int PIN_INCORRECT = -305;
	static constexpr int PIN_LOCKED = -306;
	libcdoc::result_t decryptRSA(std::vector<uint8_t> &result,
								 const std::vector<uint8_t> &data, bool oaep,
								 unsigned int idx) override final;
	libcdoc::result_t deriveConcatKDF(std::vector<uint8_t> &dst,
									  const std::vector<uint8_t> &publicKey,
									  const std::string &digest,
									  const std::vector<uint8_t> &algorithmID,
									  const std::vector<uint8_t> &partyUInfo,
									  const std::vector<uint8_t> &partyVInfo,
									  unsigned int idx) override final;
	libcdoc::result_t deriveHMACExtract(std::vector<uint8_t> &dst,
										const std::vector<uint8_t> &publicKey,
										const std::vector<uint8_t> &salt,
										unsigned int idx) override final;
	libcdoc::result_t getSecret(std::vector<uint8_t> &secret,
								unsigned int idx) override final;
	std::string getLastErrorStr(libcdoc::result_t code) const final;

	std::vector<uint8_t> secret;

	explicit DDCryptoBackend() = default;
};

//
// NetworkBackend
//
// Bridges to QNetworkAccessManager
//

struct DDNetworkBackend : public libcdoc::NetworkBackend, private QObject {
	static constexpr int BACKEND_ERROR = -303;

	std::string getLastErrorStr(libcdoc::result_t code) const final;
	libcdoc::result_t sendKey(libcdoc::NetworkBackend::CapsuleInfo &dst,
							  const std::string &url,
							  const std::vector<uint8_t> &rcpt_key,
							  const std::vector<uint8_t> &key_material,
							  const std::string &type,
							  uint64_t expiry_ts) override final;
	libcdoc::result_t
	fetchKey(std::vector<uint8_t> &result, const std::string &keyserver_id,
			 const std::string &transaction_id) override final;

	libcdoc::result_t
	getClientTLSCertificate(std::vector<uint8_t> &dst) override final {
		return libcdoc::NOT_IMPLEMENTED;
	}
	libcdoc::result_t getPeerTLSCertificates(
		std::vector<std::vector<uint8_t>> &dst) override final {
		return libcdoc::NOT_IMPLEMENTED;
	}

	explicit DDNetworkBackend() = default;

	std::string last_error;
};

//
// ILogger
//
// Bridges to Qt logging system
//

class DDCDocLogger : private libcdoc::ILogger {
  public:
	static void setUpLogger();

  private:
	DDCDocLogger() = default;
	~DDCDocLogger() = default;
	void LogMessage(libcdoc::ILogger::LogLevel level, std::string_view file, int line,
					std::string_view message) override final;
};

class CDocSupport {
public:
	static std::vector<libcdoc::FileInfo> getCDocFileList(QString filename);
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

struct TempListConsumer : public libcdoc::MultiDataConsumer {
	static constexpr int64_t MAX_VEC_SIZE = 500L * 1024L * 1024L;

	explicit TempListConsumer(size_t max_memory_size = 500L * 1024L * 1024L)
		: _max_memory_size(max_memory_size) {}
	~TempListConsumer();

	libcdoc::result_t write(const uint8_t *src, size_t size) noexcept final;
	libcdoc::result_t close() noexcept final;
	bool isError() noexcept final;
	libcdoc::result_t open(const std::string &name,
						   int64_t size) override final;

	size_t _max_memory_size;
	std::vector<IOEntry> files;
};

struct StreamListSource : public libcdoc::MultiDataSource {
	StreamListSource(const std::vector<IOEntry> &files);

	libcdoc::result_t read(uint8_t *dst, size_t size) noexcept final;
	bool isError() noexcept final;
	bool isEof() noexcept final;
	libcdoc::result_t getNumComponents() override final;
	libcdoc::result_t next(std::string &name, int64_t &size) override final;

	const std::vector<IOEntry> &_files;
	int64_t _current = -1;
};

#endif // __CDOCSUPPORT_H__
