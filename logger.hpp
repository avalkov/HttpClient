
#ifndef LOGGER_H
#define LOGGER_H

#include <algorithm>
#include <cstdlib>
#include <functional>
#include <iostream>
#include <memory>
#include <string>
#include <thread>
#include <vector>
#include <mutex>
#include <fstream>
#include <sstream>
#include <windows.h>

#include <boost/format.hpp>

#include <boost/system/error_code.hpp>

#include "config.h"

class logger
{
	
private:
	static std::mutex access_log_;

	static std::ostringstream formatLog(const char *type, const char *what);
	static std::ostringstream formatLog(const char *type, const char *what, const char *errorMsg);
	static std::streampos logger::fileSize(const char* filePath);
	static void writeLog(std::string logData);

public:
    static void fail(char const* what);
    static void fail(boost::system::error_code ec, char const* what);
    static void fail(boost::system::error_code ec, std::string what);
    static void debug(std::string what);
    
};

#endif


