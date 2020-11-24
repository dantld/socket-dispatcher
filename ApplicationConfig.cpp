/*
 * ApplicationConfig.cpp
 *
 *  Created on: 23 нояб., 2020 ел
 *      Author: dantld
 */

#include "ApplicationConfig.h"
#include <getopt.h>

class ApplicationConfigImpl : public ApplicationConfig
{
	ApplicationType _applicationType;
	bool _ok{false};
	std::string _caFile;
	std::string _certFile;
	std::string _keyFile;
	std::string _dispPath;
public:
	ApplicationConfigImpl(int argc, char *argv[], ApplicationType applicationType) :
		_applicationType(applicationType)
	{
		readCommandArguments(argc, argv);
	}

	ApplicationConfigImpl(ApplicationType applicationType) :
		_applicationType(applicationType)
	{
	}

	void readCommandArguments(int argc, char *argv[]);

	ApplicationType type() const noexcept override { return _applicationType; }

	bool isOk() const noexcept override { return _ok; }

	std::string getCaFile() const noexcept override { return _caFile; }
	std::string getCertFile() const noexcept override { return _certFile; }
	std::string getKeyFile() const noexcept override { return _keyFile; }
	std::string getDispPath() const noexcept override { return _dispPath; }

	void setCaFile(const std::string& caFile) noexcept override { _caFile = caFile; }
	void setCertFile(const std::string& certFile) noexcept override { _certFile = certFile; }
	void setKeyFile(const std::string& keyFile) noexcept override { _keyFile = keyFile; }

};

void ApplicationConfigImpl::readCommandArguments(int argc, char *argv[])
{
	int opt = 0;
	bool proceed = true;
	std::string options;
	if(_applicationType == ApplicationType::LISTENER) {
		options = "a:c:k:";
	} else if(_applicationType == ApplicationType::DISPATCHER) {
		options = "p:a:c:k:";
	}
	while( (opt = getopt(argc, argv, options.c_str())) != -1  ) {
		switch(opt) {
		case 'a':
			_caFile = optarg;
			break;
		case 'c':
			_certFile = optarg;
			break;
		case 'k':
			_keyFile = optarg;
			break;
		case 'p':
			_dispPath = optarg;
			break;
		case '-':
			proceed = false;
			break;
		default:
			proceed = false;
			fprintf(stderr,"argument reading failed\n"
					"Use:\n"
					"-a CA file\n"
					"-c Cert file\n"
					"-k Key file\n"
					);
			break;
		}
		if(!proceed) {
			break;
		}
	}

	_ok = true;

	if( _caFile.empty()   ||
		_certFile.empty() ||
		_keyFile.empty()
		) {
		_ok = false;
	}

	if(_applicationType == ApplicationType::DISPATCHER) {
		if( _dispPath.empty() ) {
			_ok = false;
		}
	}
}

ApplicationConfig::Ptr ApplicationConfig::create(int argc, char *argv[], ApplicationType applicationType)
{
	return std::make_shared<ApplicationConfigImpl>(argc,argv,applicationType);
}

ApplicationConfig::Ptr ApplicationConfig::create(ApplicationType applicationType)
{
	return std::make_shared<ApplicationConfigImpl>(applicationType);
}
