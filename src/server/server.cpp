#include <boost/asio/ip/tcp.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/version.hpp>
#include <boost/config.hpp>
#include <iostream>
#include <mqtt/client.h>
#include <boost/algorithm/string.hpp>
#include <boost/algorithm/string/classification.hpp>

namespace beast = boost::beast;
namespace http = beast::http;
namespace net = boost::asio;
using tcp = net::ip::tcp;
using namespace std;

const string HELP_MESSAGE {"Usage: server <host> <port> <mqtt-broker address>\n"};
string mqtt_host;
string path_cat(beast::string_view path) {
    string result{"."};
    char constexpr path_separator = '/';
    if(result.back() == path_separator){
        result.resize(result.size() - 1);
    }
    result.append(path.data(), path.size());
    return result;
}

vector<string> parse_req(beast::string_view path) {
    //returns vector of the form 
    // a[1] = table name a[i] are select queries for i > 1
    vector<string> res;
    boost::split(res, path, boost::is_any_of("/,"));
    return res;
}


void err(beast::error_code ec, char const* what) {
    cerr << what << ": " << ec.message() << "\n";
}

template<class Body, class Allocator, class Send>
void handle_request(http::request<Body, http::basic_fields<Allocator>>&& req, Send&& send, 
mqtt::client& client) {
    
    auto const bad_request =
    [&req](beast::string_view why) {
        http::response<http::string_body> res{http::status::bad_request, req.version()};
        res.set(http::field::server, BOOST_BEAST_VERSION_STRING);
        res.set(http::field::content_type, "text/html");
        res.keep_alive(req.keep_alive());
        res.body() = std::string(why);
        res.prepare_payload();
        return res;
    };

    auto const not_found =
    [&req](beast::string_view target) {
        http::response<http::string_body> res{http::status::not_found, req.version()};
        res.set(http::field::server, BOOST_BEAST_VERSION_STRING);
        res.set(http::field::content_type, "text/html");
        res.keep_alive(req.keep_alive());
        res.body() = "The resource '" + std::string(target) + "' was not found.";
        res.prepare_payload();
        return res;
    };
    
    auto const server_error =
    [&req](beast::string_view what) {
        http::response<http::string_body> res{http::status::internal_server_error, req.version()};
        res.set(http::field::server, BOOST_BEAST_VERSION_STRING);
        res.set(http::field::content_type, "text/html");
        res.keep_alive(req.keep_alive());
        res.body() = "An error occurred: '" + std::string(what) + "'";
        res.prepare_payload();
        return res;
    };
    
    if (req.method() != http::verb::get &&
        req.method() != http::verb::post &&
        req.method() != http::verb::delete_){
        return send(bad_request("Unknown HTTP-method"));
        }

    if (req.target().empty() ||
        req.target()[0] != '/' ||
        req.target().find("..") != beast::string_view::npos){
        return send(bad_request("Illegal request-target"));
    }
    std::string path = path_cat(req.target());
    auto pth = parse_req(req.target());
    if (req.method() == http::verb::get) {
        auto pubmsg = mqtt::make_message("get", string(req.target()));
        pubmsg->set_qos(1);
        client.publish(pubmsg);
        auto msg = client.consume_message(); 
        while (true) {
            if (msg) {
                break;
            }
        }   
        http::response<http::string_body> res{http::status::ok, req.version()};
        res.set(http::field::server, BOOST_BEAST_VERSION_STRING);
        res.set(http::field::content_type, "application/text");
        res.set(http::field::body, msg->to_string());
        res.keep_alive(req.keep_alive());
        return send(std::move(res));
    } 
    if (req.method() == http::verb::post) {
        auto pubmsg = mqtt::make_message("post", "POST");
        pubmsg->set_qos(1);
        client.publish(pubmsg);
        http::response<http::empty_body> res{http::status::created, req.version()};
        res.set(http::field::server, BOOST_BEAST_VERSION_STRING);
        
        res.keep_alive(req.keep_alive());
        return send(std::move(res));
        
    }
    if (req.method() == http::verb::delete_) {
        
    }
    
}

template<class Stream>
struct send_lambda {
    Stream& stream_;
    bool& close_;
    beast::error_code& ec_;

    explicit
    send_lambda(
        Stream& stream,
        bool& close,
        beast::error_code& ec)
        : stream_(stream)
        , close_(close)
        , ec_(ec)
    {}

    template<bool isRequest, class Body, class Fields>
    void
    operator()(http::message<isRequest, Body, Fields>&& msg) const
    {
        close_ = msg.need_eof();
        http::serializer<isRequest, Body, Fields> sr{msg};
        http::write(stream_, sr, ec_);
    }
};

void do_session(tcp::socket& socket) {
    bool close = false;
    beast::error_code ec;
    beast::flat_buffer buffer;
    send_lambda<tcp::socket> lambda{socket, close, ec};
    mqtt::client client(mqtt_host, "server");
    mqtt::connect_options connOps;
    connOps.set_keep_alive_interval(30);
    connOps.set_clean_session(true);
    client.connect(connOps);
    client.subscribe("out", 1);
    for(;;) {
        http::request<http::string_body> req;
        http::read(socket, buffer, req, ec);
        if (ec == http::error::end_of_stream){
            break;
        }
        if (ec){
            return err(ec, "read");
        }
        handle_request(std::move(req), lambda, client);
        if (ec)
            return err(ec, "write");
        if (close) {
            break;
        }
    }
    client.disconnect();
    cout << "disconnected\n";
    socket.shutdown(tcp::socket::shutdown_send, ec);

    
}

        
int main(int argc, char* argv[]) {
    try {
        if (argc != 4) {
            cerr << HELP_MESSAGE;
            return 1;
        }
        auto const address = net::ip::make_address(argv[1]);
        auto const port = static_cast<unsigned short>(atoi(argv[2]));
        mqtt_host = argv[3];
        net::io_context ioc{1};
        tcp::acceptor acceptor{ioc, {address, port}};
       
        for (;;) {
            tcp::socket socket{ioc};
            acceptor.accept(socket);
            std::thread{std::bind(&do_session,
                std::move(socket))}.detach();
        }
                
    }
    catch (const exception& e) {
        cerr << "Error: " << e.what() << "\n";
        return 1;
    }
}