
#ifndef __ACS_NTRIPSTREAM_HPP
#define __ACS_NTRIPSTREAM_HPP

#include <string>
#include <vector>
#include <chrono>

using std::string;
using std::vector;
using std::chrono::system_clock;
using std::chrono::time_point;

#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/iostreams/device/array.hpp>
#include <boost/algorithm/string/trim.hpp>
#include <boost/iostreams/stream.hpp>
#include <boost/log/expressions.hpp>
#include <boost/log/trivial.hpp>
#include <boost/filesystem.hpp>
#include <boost/format.hpp>
#include <boost/regex.hpp>
#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>


namespace io = boost::asio;
namespace ip = io::ip;
namespace ssl = io::ssl;
using tcp = ip::tcp;
using error_code = boost::system::error_code;
using ssl_socket = ssl::stream<tcp::socket>;

using namespace boost::system;

namespace B_io      = boost::iostreams;
namespace B_asio    = boost::asio;

using boost::asio::ip::tcp;


struct Base64
{
    static string encode(std::string in)
    {
        return encode(in.c_str(), in.length());
    }

    static string encode(const char * in, std::size_t len)
    {
        string out;
        const char constexpr tab[] =
        {
            "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
            "abcdefghijklmnopqrstuvwxyz"
            "0123456789+/"
        };

        for (auto n = len / 3; n--; )
        {
            out += tab[ (in[0] & 0xfc) >> 2];
            out += tab[((in[0] & 0x03) << 4) + ((in[1] & 0xf0) >> 4)];
            out += tab[((in[2] & 0xc0) >> 6) + ((in[1] & 0x0f) << 2)];
            out += tab[  in[2] & 0x3f];
            in += 3;
        }

        switch (len % 3)
        {
            case 2:
                out += tab[ (in[0] & 0xfc) >> 2];
                out += tab[((in[0] & 0x03) << 4) + ((in[1] & 0xf0) >> 4)];
                out += tab[                         (in[1] & 0x0f) << 2];
                out += '=';
                break;

            case 1:
                out += tab[ (in[0] & 0xfc) >> 2];
                out += tab[((in[0] & 0x03) << 4)];
                out += '=';
                out += '=';
                break;

            case 0:
                break;
        }

        return out;
    }
};



struct URL
{
    string  url;
    string  protocol;
    string  username;
    string  password;
    string  host;
    string  port_str;
    int     port;
    string  path;

    static URL parse(std::string url)
    {

//      boost::regex re (R"(^((https?)://)?((\w):(\w)@)?([^\s:=/]+)(:(\d+)?)/(.*$))", boost::regex::extended);
        boost::regex re (R"((https?)://(([^:@]+):([^@]+)@)?([^:@]+)(:(\d+))?(/.*)$)", boost::regex::extended);

        boost::smatch matches;

        if (!boost::regex_match(url, matches, re))
        {
            BOOST_LOG_TRIVIAL(debug) << "Invalid URL [" << url << "]";
            return URL();
        }

        BOOST_LOG_TRIVIAL(debug)
        << "Valid URL ["    << url << "]";

        BOOST_LOG_TRIVIAL(debug)
        << "protocol=["     << matches[1]
        << "] username=["   << matches[3]
        << "] password=["   << matches[4]
        << "] host=["       << matches[5]
        << "] port=["       << matches[7]
        << "] path=["       << matches[8] << "]";

        URL out;

        out.url = url;

        string protocol = matches[1];
        out.username    = matches[3];
        out.password    = matches[4];
        out.host        = matches[5];
        out.port_str    = matches[7];
        out.path        = matches[8];

        out.protocol    = protocol.     empty() ? "http"    : protocol;
        if (out.port_str.empty())
        {
            if (out.protocol == "https")    out.port_str = "443";
            else                            out.port_str = "2101";
        }
        out.port        = std::stoi(out.port_str);

        return out;
    }

    std::string sanitised()
    {
        return protocol + ":" + "//" + host + (port > 0 ? (":" + std::to_string(port)) : "") + path;
    }
};



/** Interface to be used for NTRIP streams
 */
struct NtripStream
{
private:
    B_asio::io_service          io_service;
    tcp::socket                 _socket;
    tcp::socket*                socket_ptr = &_socket;
    B_asio::ssl::context        ssl_context;
    ssl_socket                  sslsocket;
public:
    vector<char>                receivedData;
    URL                         url;
    time_point<system_clock>    lastDataTime = {};
    int                         timeout = 20;

    NtripStream() : 
        _socket     (io_service),
        ssl_context (B_asio::ssl::context::tls), 
        sslsocket   (io_service, ssl_context)
    {

    }

    void connect();

    /** Retrieve data from the stream and store it for later removal
     */
    void getData();
};

#endif
