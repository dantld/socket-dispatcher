/*
 * ApplicationConfig.h
 *
 *  Created on: 23 нояб., 2020 ел
 *      Author: dantld
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

	using Ptr = std::shared_ptr<ApplicationConfig>;

	static ApplicationConfig::Ptr create(int argc, char *argv[], ApplicationType applicationType);
	static ApplicationConfig::Ptr create(ApplicationType applicationType);
};


#endif /* APPLICATIONCONFIG_H_ */
