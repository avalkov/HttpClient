
#ifndef HTTP_RESPONSE_RECEIVER_H
#define HTTP_RESPONSE_RECEIVER_H

#include <mutex>
#include <vector>
#include <string>
#include <sstream>

#include <boost\format.hpp>

#include <rapidjson/document.h>
#include <rapidjson/writer.h>
#include <rapidjson/stringbuffer.h>

class http_response_receiver
{

public:
	void add_response(int64_t response_id, std::string response);
	void fail_request(int64_t request_id);

protected:

	static std::mutex access_responses_;
	std::vector<std::pair<int64_t, std::string>> responses_;

	explicit http_response_receiver()
	{
		responses_ = std::vector<std::pair<int64_t, std::string>>();
	}

	std::string get_response(int64_t id);
	std::string get_response_ex(int64_t id);
	bool is_existing(int64_t id);
};

#endif