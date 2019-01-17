
#include "users_listener.hpp"

users_listener::users_listener(boost::asio::io_service& ios, boost::asio::ssl::context& ctx, tcp::endpoint endpoint)
    : ios_(ios), strand_(ios), acceptor_(ios), socket_(ios), ctx_(ctx)
{
    boost::system::error_code ec;

    // Open the acceptor
    
    acceptor_.open(endpoint.protocol(), ec);
    
    if (ec)
    {
        logger::fail(ec, "open");
        
        return;
    }

    // Bind to the server address
    
    acceptor_.bind(endpoint, ec);
    
    if (ec)
    {
        logger::fail(ec, "bind");
        
        return;
    }

    // Start listening for connections
    
    acceptor_.listen(boost::asio::socket_base::max_connections, ec);
    
    if (ec)
    {
        logger::fail(ec, "listen");
        
        return;
    }
}

// Start accepting incoming connections

void users_listener::run()
{
    if (!acceptor_.is_open())
    {
        logger::fail("Acceptor not open");
        
        return;
    }
    
    do_accept();
}

void users_listener::do_accept()
{
    acceptor_.async_accept(
        socket_,
        std::bind(
            &users_listener::on_accept,
            shared_from_this(),
            std::placeholders::_1));
}

void users_listener::on_accept(boost::system::error_code ec)
{
    // Accept another connection
        
    if (ec)
    {
        logger::fail(ec, "accept");
    }
    else
    {
        logger::debug("Attempt for user connection");
    
        // Create the session and run it
        
        std::make_shared<user_client>(ios_, ctx_, std::move(socket_))->run();  
    }

    socket_ = tcp::socket(ios_);
    
    do_accept();
}
