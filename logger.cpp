
#include "logger.hpp"

std::mutex logger::access_log_;

void logger::fail(char const* what)
{
#ifdef LOGGING
	auto logData = formatLog("FAIL", what);

	writeLog(logData.str());
#endif
}

void logger::fail(boost::system::error_code ec, char const* what)
{
#ifdef LOGGING
	auto logData = formatLog("FAIL", what, ec.message().c_str());

	writeLog(logData.str());
#endif
}

void logger::fail(boost::system::error_code ec, std::string what)
{
#ifdef LOGGING
	auto logData = formatLog("FAIL", what.c_str(), ec.message().c_str());

	writeLog(logData.str());
#endif
}

void logger::debug(std::string what)
{
#ifdef LOGGING
	auto logData = formatLog("DEBUG", what.c_str());

	writeLog(logData.str());
#endif
}

std::ostringstream logger::formatLog(const char *type, const char *what)
{
	std::ostringstream slogData;

	slogData << boost::format("\r\n[%s] - %s\r\n") % type % what;

	return slogData;
}

std::ostringstream logger::formatLog(const char *type, const char *what, const char *errorMsg)
{
	std::ostringstream slogData;

	slogData << boost::format("\r\n[%s] - %s: %s\r\n") % type % what % errorMsg;

	return slogData;
}

void logger::writeLog(std::string logData)
{
	const char *logFileName = "log.txt";

	std::lock_guard<std::mutex> lock(access_log_);

	// If file size exceeds 5MB then erase the log and start all over
	
	if (fileSize(logFileName) > 5000000)
	{
		std::remove(logFileName);
	}

	std::ofstream ofs;
	ofs.open(logFileName, std::ofstream::out | std::ofstream::app);
	std::cout << std::endl << logData << std::endl;
	ofs << logData;
	ofs.close();
}

std::streampos logger::fileSize(const char* filePath)
{
	std::streampos fsize = 0;
	std::ifstream file(filePath, std::ios::binary);

	fsize = file.tellg();
	file.seekg(0, std::ios::end);
	fsize = file.tellg() - fsize;
	file.close();

	return fsize;
}