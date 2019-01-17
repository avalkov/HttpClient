
#ifndef USER_CLIENT_HPP
#define USER_CLIENT_HPP

#include <algorithm>
#include <cstdlib>
#include <functional>
#include <iostream>
#include <memory>
#include <string>
#include <thread>
#include <vector>
#include <deque>
#include <mutex>
#include <queue>
#include <chrono>

#include <boost/format.hpp>

#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/asio/strand.hpp>
#include <boost/asio/ip/tcp.hpp>

#include <boost/beast/core.hpp>
#include <boost/beast/websocket.hpp>

#include <boost/algorithm/string.hpp>

#include <rapidjson/document.h>

#include "logger.hpp"
#include "http_response_receiver.hpp"
#include "http_requester.hpp"
#include "time_helpers.hpp"
#include "config.h"

namespace websocket = boost::beast::websocket;
using tcp = boost::asio::ip::tcp;

class user_client : http_response_receiver, public std::enable_shared_from_this<user_client>
{
    
private:
   
	const char *send_params_[8] = { "method", "contentType", "host", "port", "uri", "data", "headers", 0x00 };

    boost::asio::io_service& ios_;
	boost::asio::ssl::context& ctx_;
    boost::beast::websocket::stream<tcp::socket> ws_;
    boost::asio::io_service::strand strand_;
    boost::beast::multi_buffer buffer_;
    std::string send_data_;
	boost::asio::deadline_timer check_requests_timer_;
	bool is_writing_;
	bool is_first_read_;

	bool validate_request_parameters(rapidjson::Document &request_doc, const char* params_to_check[]);
	void handle_request(std::string request);
	void run_check_requests_timer();
	void check_requests();

public:
    
    explicit user_client(boost::asio::io_service& ios, boost::asio::ssl::context& ctx, tcp::socket socket)
        : http_response_receiver(), ios_(ios), ctx_(ctx), strand_(ios), ws_(std::move(socket)),
		check_requests_timer_(ios, boost::posix_time::seconds(1))
    {
		is_first_read_ = true;
	}
        
    void run();
    void on_accept(boost::system::error_code ec);
    void do_read();
    void on_read(boost::system::error_code ec, std::size_t bytes_transferred);
    void on_write(boost::system::error_code ec, std::size_t bytes_transferred);
    void on_close(boost::system::error_code ec);
    void disconnect();
};

#endif

