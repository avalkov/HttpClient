
#ifndef HTTP_REQUESTER_HPP
#define HTTP_REQUESTER_HPP

#include <algorithm>
#include <cstdlib>
#include <functional>
#include <iostream>
#include <istream>
#include <ostream>
#include <memory>
#include <string>
#include <thread>
#include <vector>
#include <queue>
#include <mutex>

#include <boost/bind.hpp>

#include <boost/asio/strand.hpp>
#include <boost/asio/connect.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/asio/ssl/error.hpp>
#include <boost/asio/ssl/stream.hpp>

#include <boost/algorithm/string.hpp>
#include <boost/algorithm/string/split.hpp>

#include <boost/range/algorithm/remove_if.hpp>

#include <boost/range/algorithm_ext/erase.hpp>
#include <boost/range/algorithm_ext.hpp>

#include "logger.hpp"
#include "http_response_receiver.hpp"
#include "time_helpers.hpp"

using tcp = boost::asio::ip::tcp;

class http_requester : public std::enable_shared_from_this<http_requester>
{
    
private:

	const char TERMINATING_CHUNK[8] = "\r\n0\r\n\r\n";

    boost::asio::io_service& ios_;
    boost::asio::ssl::context& ctx_;
    tcp::resolver resolver_;
    boost::asio::ssl::stream<boost::asio::ip::tcp::socket> socket_;
	std::string method_;
	std::string content_type_;
    std::string host_;
    std::string port_;
    std::string uri_;
    std::string data_;
    std::string additional_headers_;
    boost::asio::streambuf request_;
    boost::asio::streambuf response_;
    std::string all_response_;
    
	int64_t response_id_;
	http_response_receiver* http_response_receiver_;
	unsigned int content_length_;
	bool is_chunked_transfer_;
	unsigned int current_chunk_length_;
	bool response_ready_;

	boost::asio::deadline_timer handshake_timer_;
	boost::asio::deadline_timer request_timer_;

	long long start_;
	long long end_;

	std::vector<int> big_vector_;

	void handshake_timeout(const boost::system::error_code& ec);
	void request_timeout(const boost::system::error_code& ec);

public:
    explicit http_requester(boost::asio::io_service& ios, boost::asio::ssl::context& ctx, std::string method, 
		std::string content_type, std::string host, std::string port, std::string uri, std::string data,
		std::string additional_headers, int64_t response_id, http_response_receiver* http_response_receiver)
        : ios_(ios), ctx_(ctx), resolver_(ios), socket_(ios, ctx), method_(method), content_type_(content_type), host_(host), port_(port),
                uri_(uri), data_(data), additional_headers_(additional_headers),
				response_id_(response_id), http_response_receiver_(http_response_receiver), handshake_timer_(ios), request_timer_(ios)
    {
		content_length_ = 0;
		is_chunked_transfer_ = false;
		current_chunk_length_ = 0;
		response_ready_ = false;
    }
    
    void run();
    void on_resolve(boost::system::error_code ec, tcp::resolver::iterator endpoints);
    void on_handshake(const boost::system::error_code& ec);
    void on_connect(const boost::system::error_code& err);
    void on_write(boost::system::error_code ec);
    void handle_read_status_line(const boost::system::error_code& err);
    void handle_read_headers(const boost::system::error_code& err);
    void handle_read_content(const boost::system::error_code& err);
    bool check_response();
	bool is_terminating_chunk_received();
	bool decode_chunked_response();
};

#endif 

