
// Accepts incoming connections and launches the sessions

#ifndef LISTENER_HPP
#define LISTENER_HPP

#include <algorithm>
#include <cstdlib>
#include <functional>
#include <iostream>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#include <boost/beast/core.hpp>
#include <boost/beast/websocket.hpp>

#include <boost/asio/strand.hpp>

#include <boost/asio/ip/tcp.hpp>

#include <boost/asio/ssl.hpp>

#include "logger.hpp"
#include "user_client.hpp"

namespace websocket = boost::beast::websocket;
using tcp = boost::asio::ip::tcp;

class users_listener : public std::enable_shared_from_this<users_listener>
{
    tcp::acceptor acceptor_;
    tcp::socket socket_;
    boost::asio::io_service& ios_;
	boost::asio::ssl::context& ctx_;
    boost::asio::io_service::strand strand_;
    
public:
    users_listener(boost::asio::io_service& ios, boost::asio::ssl::context& ctx, tcp::endpoint endpoint);
    void run();
    void do_accept();
    void on_accept(boost::system::error_code ec);
};

#endif

