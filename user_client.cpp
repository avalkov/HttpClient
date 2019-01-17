
#include <queue>

#include "user_client.hpp"

void user_client::run()
{
    // Accept the web socket handshake

    ws_.async_accept(
        strand_.wrap(
        std::bind(
            &user_client::on_accept,
            shared_from_this(),
            std::placeholders::_1)));
}

void user_client::on_accept(boost::system::error_code ec)
{
    if (ec)
    {
        return logger::fail(ec, "accept");
    }

    do_read();
}

void user_client::do_read()
{
    ws_.async_read(
        buffer_,
        strand_.wrap(
        std::bind(
            &user_client::on_read,
            shared_from_this(),
            std::placeholders::_1,
            std::placeholders::_2)));
}

void user_client::on_read(boost::system::error_code ec, std::size_t bytes_transferred)
{
    boost::ignore_unused(bytes_transferred);

    // This indicates that the session was closed

    if (ec == websocket::error::closed)
    {
		check_requests_timer_.cancel();

        return;
    }

    if (ec)
    {
        logger::fail(ec, "read");
        
		disconnect();
        
        return;
    }

	try
	{
		if (is_first_read_ == true)
		{
			is_first_read_ = false;

			run_check_requests_timer();
		}

		auto received_buffer = boost::beast::buffers(buffer_.data());
		std::stringstream sreceived;
		sreceived << received_buffer;

		// Clear the buffer

		buffer_.consume(buffer_.size());

		handle_request(sreceived.str());
	}
	catch (std::exception& ex)
	{
		std::ostringstream swhat;

		swhat << boost::format("Error request: %s") % ex.what();

		logger::fail(swhat.str().c_str());
	}
}

void user_client::handle_request(std::string request)
{
	std::string response;
	rapidjson::Document request_doc;

	bool is_valid_request = false;

	logger::debug(request);

	request_doc.Parse(request.c_str());

	if (request_doc.HasMember("type") == true && request_doc["type"].IsString() == true 
		&& request_doc.HasMember("id") == true && request_doc["id"].IsInt64() == true)
	{
		auto request_type = request_doc["type"].GetString();
		auto response_id = request_doc["id"].GetInt64();

		if (boost::iequals(request_type, "send") == true && validate_request_parameters(request_doc, send_params_) == true)
		{
			bool is_valid = true;

			for (int i = 0; send_params_[i] != 0x00; i++)
			{
				if (request_doc.HasMember(send_params_[i]) == false || request_doc[send_params_[i]].IsString() == false)
				{
					is_valid = false;

					break;
				}
			}

			if (is_valid == true)
			{
				is_valid_request = true;

				add_response(response_id, "");

				std::make_shared<http_requester>(ios_, ctx_, request_doc["method"].GetString(), request_doc["contentType"].GetString(), request_doc["host"].GetString(),
					request_doc["port"].GetString(), request_doc["uri"].GetString(), request_doc["data"].GetString(), request_doc["headers"].GetString(),
					response_id, static_cast<http_response_receiver*>(this))->run();

				std::ostringstream sresponse;

				sresponse << boost::format("{ \"status\": \"Sent\", \"id\": %d }") % response_id;
				response = sresponse.str();
			}
		}
		else if (boost::iequals(request_type, "ping") == true)
		{
			std::ostringstream sresponse;

			sresponse << boost::format("{ \"status\": \"pong\", \"id\": %d }") % response_id;
			response = sresponse.str();

			responses_.push_back(std::pair<int64_t, std::string>(response_id, response));
		}
	}

	if (is_valid_request == false || response.empty())
	{
		response = "{ \"status\": \"Invalid\" }";
	}

	do_read();
}

void user_client::run_check_requests_timer()
{
	check_requests_timer_.expires_at(check_requests_timer_.expires_at() + boost::posix_time::milliseconds(20));
	
	check_requests_timer_.async_wait(
		strand_.wrap(
			std::bind(&user_client::check_requests,
				shared_from_this())));
}

void user_client::check_requests()
{
	auto timestamp = get_milliseconds_unix_timestamp();

	std::string response;

	std::lock_guard<std::mutex> lock(access_responses_);

	for (int i = 0; i < responses_.size(); i++)
	{
		if (timestamp - responses_[i].first > REQUEST_TIMEOUT * 1000)
		{
			std::ostringstream sresponse;

			sresponse << boost::format("{ \"status\": \"Timedout\", \"id\": %d }") % responses_[i].first;
			response = sresponse.str();

			responses_.erase(responses_.begin() + i);

			break;
		}

		if (is_writing_ == true)
		{
			break;
		}

		response = get_response_ex(responses_[i].first);

		if (response.empty() == false)
		{
			responses_.erase(responses_.begin() + i);

			break;
		}
	}

	if (response.empty() == false)
	{
		is_writing_ = true;

		logger::debug(response);

		send_data_ = response;

		ws_.async_write(
			boost::asio::buffer(send_data_),
			strand_.wrap(
				std::bind(
					&user_client::on_write,
					shared_from_this(),
					std::placeholders::_1,
					std::placeholders::_2)));
	}

	run_check_requests_timer();
}

bool user_client::validate_request_parameters(rapidjson::Document &request_doc, const char* params_to_check[])
{
	bool is_valid = true;

	for (int i = 0; params_to_check[i] != 0x00; i++)
	{
		if (request_doc.HasMember(params_to_check[i]) == false || request_doc[params_to_check[i]].IsString() == false)
		{
			is_valid = false;

			break;
		}
	}

	return is_valid;
}

void user_client::on_write(boost::system::error_code ec, std::size_t bytes_transferred)
{
    boost::ignore_unused(bytes_transferred);

    if (ec)
    {
        ws_.async_close(websocket::close_code::normal,
            strand_.wrap(
            std::bind(
                &user_client::on_close,
                shared_from_this(),
                std::placeholders::_1)));
                
        logger::fail(ec, "write");

		is_writing_ = false;
        
        return;
    }
	
	is_writing_ = false;
}

void user_client::disconnect()
{
	ws_.async_close(websocket::close_code::normal,
		strand_.wrap(
			std::bind(
				&user_client::on_close,
				shared_from_this(),
				std::placeholders::_1)));
}

void user_client::on_close(boost::system::error_code ec)
{
	check_requests_timer_.cancel();

    // WebSocket says that to close a connection you have
    // to keep reading messages until you receive a close frame.
    // Beast delivers the close frame as an error from read.
    
    for(;;)
    {
        // Keep reading messages...
        
        ws_.read(buffer_, ec);

        // Clear the buffer

        buffer_.consume(buffer_.size());

        // ...until we get the special error code
        
        if (ec == boost::beast::websocket::error::closed)
        {
            break;
        }
        
        // Some other error occurred, report it and exit.
        
        if (ec)
        {
            return logger::fail(ec, "close");
        }
    }
}