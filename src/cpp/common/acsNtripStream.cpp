
#include "acsNtripStream.hpp"

#include <boost/system/error_code.hpp>
#include <list>

using std::list;

void NtripStream::connect()
{
	std::cout << "(Re)connecting " << url.sanitised() << std::endl;

	try
	{
		// Get a list of endpoints corresponding to the server name.
		tcp::resolver				resolver(io_service);
// 		tcp::resolver::query		query(url.host, url.port_str);		//slow
		tcp::resolver::query		query(boost::asio::ip::tcp::v4(), url.host, url.port_str);
		tcp::resolver::iterator		endpoint_iterator = resolver.resolve(query);

		// Try each endpoint until we successfully establish a connection.
		if (url.protocol == "https")	socket_ptr = &sslsocket.next_layer();
		else 							socket_ptr = &_socket;
		
		auto& socket = *socket_ptr;
		B_asio::connect(socket, endpoint_iterator);
		
		if (url.protocol == "https")
			sslsocket.handshake(ssl::stream_base::client);
		
		B_asio::streambuf	request;
		std::ostream				request_stream(&request);

									request_stream	<< "GET " 		<< url.path << " HTTP/1.0\r\n";
									request_stream	<< "User-Agent: NTRIP ACS/1.0\r\n";
									request_stream	<< "Host: " 	<< url.host;
									request_stream	<< "\r\n";
		if (!url.username.empty())	request_stream	<< "Authorization: Basic "
													<< Base64::encode(string(url.username + ":" + url.password))
													<< "\r\n";
									request_stream	<< "\r\n";
									
								

		// Send the request.
		if (url.protocol == "https")	B_asio::write(sslsocket,	request);
		else							B_asio::write(socket,		request);

		// Read the response status line. The response streambuf will automatically
		// grow to accommodate the entire line. The growth may be limited by passing
		// a maximum size to the streambuf constructor.

		B_asio::streambuf	response;
		if (url.protocol == "https")	B_asio::read_until(sslsocket,	response, "\r\n");
		else							B_asio::read_until(socket,		response, "\r\n");

		std::istream	response_stream(&response);

		while (1)
		{
			string line;
			std::getline(response_stream, line);
			boost::algorithm::trim_right(line);
			if (line.empty())
			{
				break;
			}

			lastDataTime = system_clock::now();
// 			BOOST_LOG_TRIVIAL(info)
// 			<< "Read line [" << line.length()
// 			<< "]=[" << line << "]";
		}

		// the timeout value
		struct timeval timeout;
		timeout.tv_sec	= 5;
		timeout.tv_usec	= 0;
		setsockopt(socket.native_handle(), SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
		setsockopt(socket.native_handle(), SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));
		
		boost::asio::socket_base::keep_alive keepAliveOption(true);
		socket.set_option(keepAliveOption);
	}
	catch (std::exception& e)
	{
		std::cout << "NTRIP Exception: " << e.what() << "\n";
	}
}

void NtripStream::getData()
{
	boost::system::error_code	error;

	list<vector<char>>	responseList;
	int totalCount = 0;
	
	while (1)
	{
		long int available = socket_ptr->available(error);
		if (available == 0)
		{
			//if (error)
			//	std::cout << error.category().name() << error.category().message(error.value()) << '\n';

			if (system_clock::now() > lastDataTime + std::chrono::milliseconds(timeout * 1000))
			{
// 				std::cout << "Timeout" << std::endl;
				lastDataTime = system_clock::now();
				connect();
			}

			break;
		}


		B_asio::streambuf	response;
		vector<char> vecc;
		vecc.resize(1024);
		int count;
		if (url.protocol == "https")	count = boost::asio::read(sslsocket,	boost::asio::buffer(vecc.data(), vecc.size()), boost::asio::transfer_at_least(1), error);
		else 							count = boost::asio::read(*socket_ptr,	boost::asio::buffer(vecc.data(), vecc.size()), boost::asio::transfer_at_least(1), error);
		
		if (count == 0)
		{
			break;
		}
		vecc.resize(count);

		lastDataTime = system_clock::now();

		totalCount += count;
		responseList.push_back(vecc);
// 		if (error != boost::asio::error::eof)
// 			throw boost::system::system_error(error);
	}
	receivedData.reserve(totalCount);

	for (auto& a : responseList)
	{
		receivedData.insert(receivedData.end(), a.begin(), a.end());
	}
}
