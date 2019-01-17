
#include "http_response_receiver.hpp"

std::mutex http_response_receiver::access_responses_;

void http_response_receiver::add_response(int64_t response_id, std::string response)
{
	std::lock_guard<std::mutex> lock(access_responses_);

	if (response.empty() == true)
	{
		responses_.push_back(std::pair<int64_t, std::string>(response_id, response));
	}
	else
	{
		for (unsigned int i = 0; i < responses_.size(); i++)
		{
			if (responses_[i].first == response_id)
			{
				responses_[i].second = response;

				break;
			}
		}
	}
}

void http_response_receiver::fail_request(int64_t request_id)
{
	std::ostringstream sresponse;

	sresponse << boost::format("{ \"status\": \"FailRetry\", \"id\": %d }") % request_id;
	auto response = sresponse.str();

	add_response(request_id, response);
}

std::string http_response_receiver::get_response_ex(int64_t id)
{
	std::string response;

	if (responses_.empty() == false)
	{
		std::pair<int64_t, std::string> entry;

		bool is_matched = false;

		for (auto const& value : responses_)
		{
			if (value.first == id && value.second.empty() == false)
			{
				is_matched = true;

				entry = value;
			}
		}

		if (is_matched == false)
		{
			return response;
		}

		rapidjson::Document document;

		document.SetObject();

		rapidjson::Document::AllocatorType& allocator = document.GetAllocator();

		rapidjson::Value js_response(rapidjson::kStringType);

		js_response.SetString(entry.second.c_str(), allocator);

		document.AddMember("status", "Finished", allocator);
		document.AddMember("id", id, allocator);
		document.AddMember("response", js_response, allocator);

		rapidjson::StringBuffer strbuf;

		rapidjson::Writer<rapidjson::StringBuffer> writer(strbuf);

		document.Accept(writer);

		response = strbuf.GetString();
	}

	return response;
}

std::string http_response_receiver::get_response(int64_t id)
{
	std::string response;

	std::lock_guard<std::mutex> lock(access_responses_);

	response = get_response_ex(id);

	return response;
}

bool http_response_receiver::is_existing(int64_t id)
{
	bool is_matched = false;

	for (auto const& value : responses_)
	{
		if (value.first == id)
		{
			is_matched = true;
		}
	}

	return is_matched;
}
