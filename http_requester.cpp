#include "http_requester.hpp"

void http_requester::run()
{
	start_ = get_milliseconds_unix_timestamp();

	request_timer_.expires_from_now(boost::posix_time::seconds(60));
	request_timer_.async_wait(
		std::bind(&http_requester::request_timeout, shared_from_this(),
			std::placeholders::_1));

    // Look up the domain name
    
    tcp::resolver::query query(host_, port_, boost::asio::ip::resolver_query_base::all_matching);
    
    logger::debug(host_ + ":" + port_);

	// Set SNI Hostname (many hosts need this to handshake successfully)

	if (!SSL_set_tlsext_host_name(socket_.native_handle(), host_.c_str()))
	{
		boost::system::error_code ec{ static_cast<int>(::ERR_get_error()), boost::asio::error::get_ssl_category() };
		logger::fail(ec, "SSL_set_tlsext_host_name");
		return;
	}

    resolver_.async_resolve(
        query,
        std::bind(
            &http_requester::on_resolve,
            shared_from_this(),
            std::placeholders::_1,
            std::placeholders::_2)); 
}

void http_requester::on_resolve(boost::system::error_code ec, tcp::resolver::iterator endpoints)
{
    if (ec)
    {
        logger::fail(ec, "resolve");
        
        return;
    }
    
    socket_.set_verify_mode(boost::asio::ssl::verify_peer);
	socket_.set_verify_callback([](bool p, boost::asio::ssl::verify_context& context) {
		return true;
	});

    // Make the connection on the IP address we get from a lookup

    boost::asio::async_connect(socket_.lowest_layer(), endpoints,
                               std::bind(&http_requester::on_connect, shared_from_this(),
                                         std::placeholders::_1));
}

void http_requester::on_connect(const boost::system::error_code& err)
{
    if (!err)
    {
		handshake_timer_.expires_from_now(boost::posix_time::milliseconds(2000));
		handshake_timer_.async_wait(
			std::bind(&http_requester::handshake_timeout, shared_from_this(), 
				std::placeholders::_1));

		socket_.async_handshake(boost::asio::ssl::stream_base::client,
			std::bind(&http_requester::on_handshake, shared_from_this(),
				std::placeholders::_1));
    }
    else
    {
		logger::fail(err, "on_connect");
    }
}

void http_requester::handshake_timeout(const boost::system::error_code& ec)
{
	if (ec != boost::asio::error::operation_aborted)
	{
		logger::fail("Handshake time out");

		try
		{
			request_timer_.cancel();

			socket_.lowest_layer().shutdown(socket_.lowest_layer().shutdown_both);
			socket_.lowest_layer().close();
		}
		catch (std::exception& ex)
		{

		}
	}

	return;
}

void http_requester::on_handshake(const boost::system::error_code& ec)
{
	handshake_timer_.cancel();

    if (ec)
    {
        logger::fail(ec, "on_handshake");

		try
		{
			socket_.lowest_layer().shutdown(socket_.lowest_layer().shutdown_both);
			socket_.lowest_layer().close();
		}
		catch (std::exception& ex)
		{

		}
		
		http_response_receiver_->fail_request(response_id_);

        return;
    }
      
    std::ostream request_stream(&request_);
    request_stream << method_ << " " << uri_ << " HTTP/1.1\r\n";
    request_stream << "Host: " << host_ << "\r\n";
	request_stream << "Accept: */*\r\n";

	if (method_ == "POST" || method_ == "PUT")
	{
		request_stream << "Content-Type: " << content_type_ << "\r\n";
		request_stream << "Content-Length: " << data_.length() << "\r\n";
	}

    if (additional_headers_.length() > 0)
    {
        request_stream << additional_headers_;
    }
    
    request_stream << "Connection: close\r\n\r\n";
	
	if (method_ == "POST" || method_ == "PUT")
	{
		request_stream << data_;
	}

    // Send the HTTP request to the remote host
  
    boost::asio::async_write(socket_, request_,
        std::bind(
            &http_requester::on_write,
            shared_from_this(),
            std::placeholders::_1));
}

void http_requester::on_write(boost::system::error_code ec)
{
    if (ec)
    {
        return logger::fail(ec, "on_write");
    }
    
    // Read the response status line.

    boost::asio::async_read_until(socket_, response_, "\r\n",
        std::bind(&http_requester::handle_read_status_line, shared_from_this(),
          std::placeholders::_1));
}

void http_requester::handle_read_status_line(const boost::system::error_code& err)
{
    if (!err)
    {
        // Check that response is OK.

        std::istream response_stream(&response_);
        std::string http_version;

        response_stream >> http_version;

        unsigned int status_code;

        response_stream >> status_code;

		if (status_code == 404)
		{
			content_length_ = 1;
			all_response_.append("{ \"error\": \"Page not found\" }");

			if (check_response() == false)
			{
				return;
			}
		}

        std::string status_message;
        std::getline(response_stream, status_message);

        if (!response_stream || http_version.substr(0, 5) != "HTTP/")
        {
          logger::fail("handle_read_status_line: Invalid response\n");

          return;
        }
        
        /*
        if (status_code != 200)
        {
          logger::fail("Response returned with status code " + status_code);
          
          return;
        }
         */
        
        // Read the response headers, which are terminated by a blank line.

        boost::asio::async_read_until(socket_, response_, "\r\n\r\n",
            std::bind(&http_requester::handle_read_headers, shared_from_this(),
              std::placeholders::_1));
    }
    else
    {
        //	std::istream response_stream(&response_);
        //	std::ostringstream ss;
		//	
        //	ss << response_stream.rdbuf();
        //	
        //	std::cout << ss.str();
        
        logger::fail(err, "handle_read_status_line");
    }
}

void http_requester::handle_read_headers(const boost::system::error_code& err)
{
  if (!err)
  {
    // Process the response headers.
      
    std::istream response_stream(&response_);
    std::string header;
    
    while (std::getline(response_stream, header) && header != "\r")
    {
		std::vector<std::string> header_parts;
		boost::range::remove_erase_if(header, boost::is_any_of("\r\n"));
		boost::split(header_parts, header, boost::is_any_of(" "));

		if (header_parts.size() == 2 && boost::iequals(header_parts[0], "Content-Length:") == true)
		{
			content_length_ = std::atoi(header_parts[1].c_str());
		}
		else if (header_parts.size() == 2 && boost::iequals(header_parts[0], "Transfer-Encoding:") == true 
			&& boost::iequals(header_parts[1], "chunked") == true)
		{
			is_chunked_transfer_ = true;
		}
    }

    // Write whatever content we already have to output.
    
    if (response_.size() > 0)
    {
        std::istream is(&response_);

        std::string s(std::istreambuf_iterator<char>(is), {});
        
		if (is_chunked_transfer_ == false)
		{
			int start_trim_pos = s.find('\n');

			if (start_trim_pos != std::string::npos)
			{
				auto lengthPrepend = s.substr(0, start_trim_pos - 1);

				content_length_ = std::strtol(lengthPrepend.c_str(), NULL, 16);

				s.erase(0, start_trim_pos + 1);
			}

			int end_trim_pos = s.find('\r');

			if (end_trim_pos != std::string::npos)
			{
				s.erase(end_trim_pos, s.length() - end_trim_pos);
			}
		}

        all_response_.append(s);
    }
    
    if (check_response() == false)
    {
        return;
    }
    
    // Start reading remaining data until EOF.
    
    boost::asio::async_read(socket_, response_,
        boost::asio::transfer_at_least(1),
        std::bind(&http_requester::handle_read_content, shared_from_this(),
          std::placeholders::_1));
  }
  else
  {
      logger::fail(err, "handle_read_headers");
  }
}

void http_requester::handle_read_content(const boost::system::error_code& err)
{
    if (!err)
    {
        // Write all of the data that has been read so far.

        std::istream is(&response_);

        std::string s(std::istreambuf_iterator<char>(is), {});
        
        all_response_.append(s);

		if (check_response() == false)
		{
			return;
		}

        // Continue reading remaining data until EOF.
        
        boost::asio::async_read(socket_, response_,
            boost::asio::transfer_at_least(1),
            std::bind(&http_requester::handle_read_content, shared_from_this(),
              std::placeholders::_1));
    }
    else
    {
		if (is_chunked_transfer_ == true)
		{
			if (is_terminating_chunk_received() == true)
			{
				try
				{
					decode_chunked_response();

					response_ready_ = true;

					check_response();
				}
				catch (std::exception& ex)
				{
					std::ostringstream swhat;

					swhat << boost::format("Error decoding chunked content: %s") % ex.what();

					logger::fail(swhat.str().c_str());

					return;
				}
			}
		}
		else
		{
			if (err == boost::asio::error::eof || err == boost::asio::error::bad_descriptor)
			{
				if (check_response() == false)
				{
					return;
				}
			}
		}

		logger::fail(err, "handle_read_content");
    }
}

bool http_requester::is_terminating_chunk_received()
{
	int terminating_chunk_len = std::strlen(TERMINATING_CHUNK);

	if (all_response_.length() < terminating_chunk_len)
	{
		return false;
	}

	auto terminating_chunk = all_response_.substr(all_response_.length() - terminating_chunk_len, terminating_chunk_len);

	if (terminating_chunk.compare(TERMINATING_CHUNK) == 0)
	{
		return true;
	}

	return false;
}

bool http_requester::decode_chunked_response()
{
	bool is_last_chunk_reached = false;
	
	// TODO: Some additional checks can be added

	int current_offset = 0;

	while (1)
	{
		int start_trim_pos = all_response_.find('\r\n', current_offset);

		if (start_trim_pos != std::string::npos)
		{
			auto length = all_response_.substr(current_offset, start_trim_pos - current_offset);

			auto chunk_length = std::strtol(length.c_str(), NULL, 16);

			if (chunk_length == 0)
			{
				all_response_.erase(current_offset, (start_trim_pos + 4) - current_offset);

				is_last_chunk_reached = true;

				break;
			}

			all_response_.erase(current_offset, (start_trim_pos + 2) - current_offset);

			current_offset += chunk_length;

			all_response_.erase(current_offset, 2);
		}
	}

	return is_last_chunk_reached;
}

void http_requester::request_timeout(const boost::system::error_code& ec)
{
	if (ec != boost::asio::error::operation_aborted)
	{
		logger::fail("Request time out");

		try
		{
			socket_.lowest_layer().shutdown(socket_.lowest_layer().shutdown_both);
			socket_.lowest_layer().close();
		}
		catch (std::exception& ex)
		{

		}

		http_response_receiver_->fail_request(response_id_);
	}

	return;
}

bool http_requester::check_response()
{
    if ((content_length_ != 0 && all_response_.length() >= content_length_) || response_ready_ == true)
    {
        logger::debug("All response: " + all_response_);

		try
		{
			http_response_receiver_->add_response(response_id_, all_response_);

			request_timer_.cancel();

			// socket_.lowest_layer().cancel(); // TODO: Is that ok ?
			socket_.lowest_layer().shutdown(socket_.lowest_layer().shutdown_both);
			socket_.lowest_layer().close();
		}
		catch (std::exception& ex)
		{
			std::ostringstream swhat;

			swhat << boost::format("Error closing socket: %s") % ex.what();

			logger::fail(swhat.str().c_str());
		}

        return false;
    }
    
    return true;
}

