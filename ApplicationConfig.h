/*
 * Copyright (c) 2022 Daniyar Tleulin <daniyar.tleulin@gmail.com>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF MIND, USE, DATA OR PROFITS, WHETHER
 * IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING
 * OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#ifndef APPLICATIONCONFIG_H_
#define APPLICATIONCONFIG_H_

#include <string>
#include <memory>

enum class ApplicationType {
	LISTENER,
	DISPATCHER
};

class ApplicationConfig {
public:
	ApplicationConfig() = default;
	virtual ~ApplicationConfig() {}

	virtual ApplicationType type() const noexcept = 0;

	virtual bool isOk() const noexcept = 0;

	virtual std::string getCaFile() const noexcept = 0;
	virtual std::string getCertFile() const noexcept = 0;
	virtual std::string getKeyFile() const noexcept = 0;
	virtual std::string getDispPath() const noexcept = 0;

	virtual void setCaFile(const std::string& caFile) noexcept = 0;
	virtual void setCertFile(const std::string& certFile) noexcept = 0;
	virtual void setKeyFile(const std::string& keyFile) noexcept = 0;

	/// @brief Method must read configuration option in form: NAME=VALUE.
	virtual void retrieveConfigOption(const char* configLine) noexcept = 0;

	using Ptr = std::shared_ptr<ApplicationConfig>;

	static ApplicationConfig::Ptr create(int argc, char *argv[], ApplicationType applicationType);
	static ApplicationConfig::Ptr create(ApplicationType applicationType);
};


#endif /* APPLICATIONCONFIG_H_ */
