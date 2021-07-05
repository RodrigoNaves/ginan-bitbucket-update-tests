
#ifndef ACS_STREAM_H
#define ACS_STREAM_H

#include <iostream>
#include <sstream>
#include <utility>
#include <vector>
#include <string>
#include <thread>
#include <list>
#include <map>


using std::string;
using std::tuple;
using std::list;
using std::pair;
using std::map;






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

namespace B_io		= boost::iostreams;
namespace B_asio	= boost::asio;

using boost::asio::ip::tcp;

#ifndef WIN32
#	include <dirent.h>
#	include <time.h>
#	include <sys/time.h>
#	include <sys/stat.h>
#	include <sys/types.h>
#endif

#include "observations.hpp"
#include "navigation.hpp"
#include "constants.h"
#include "station.hpp"
#include "common.hpp"
#include "gaTime.hpp"
#include "rinex.hpp"
#include "enum.h"



//interfaces

/** Interface for streams that supply observations
 */
struct ObsStream
{
	RinexStation	rnxStation = {};
	list<ObsList>	obsListList;	
	string			sourceString;

	/** Return a list of observations from the stream.
	 * This function may be overridden by objects that use this interface
	 */
	virtual ObsList getObs();

	/** Return a list of observations from the stream, with a specified timestamp.
	 * This function may be overridden by objects that use this interface
	 */
	ObsList getObs(
		GTime	time,			///< Timestamp to get observations for
		double	delta = 0.5)	///< Acceptable tolerance around requested time
	{
		while (1)
		{
			ObsList obsList = getObs();

			if (obsList.size() == 0)
			{
				return obsList;
			}

			if (time == GTime::noTime())
			{
				return obsList;
			}

			if		(obsList.front().time < time - delta)
			{
				eatObs();
			}
			else if	(obsList.front().time > time + delta)
			{
				return ObsList();
			}
			else
			{
				return obsList;
			}
		}
	}


	/** Check to see if this stream has run out of data
	 */
	virtual bool isDead()
	{
		return false;
	}

	/** Remove some observations from memory
	 */
	void eatObs()
	{
		if (obsListList.size() > 0)
		{
			obsListList.pop_front();
		}
	}
};


/** Interface for objects that provide navigation data
 */
struct NavStream
{
	virtual void getNav()
	{

	}
};




#include "acsFileStream.hpp"
#include "acsRtcmStream.hpp"
#include "acsRinexStream.hpp"
#include "acsNtripStream.hpp"


/** Object that streams RTCM data from NTRIP castors.
 * Overrides interface functions
 */
struct NtripRtcmStream : NtripStream, RtcmStream
{

	NtripRtcmStream(const string& url_str)
	{
		url = URL::parse(url_str);		
		sourceString = url_str;
		open();
	}

	void setUrl(const string& url_str)
	{		
		sourceString = url_str;
		url = URL::parse(url_str);
	}

	void open()
	{
// 		connect();
	}

	ObsList getObs()
	{
		//get all data available from the ntrip stream and convert it to an iostream for processing
		getData();

		B_io::basic_array_source<char>					input_source(receivedData.data(), receivedData.size());
		B_io::stream<B_io::basic_array_source<char>>	inputStream(input_source);

		parseRTCM(inputStream);

		int pos = inputStream.tellg();
		if (pos < 0)
		{
			receivedData.clear();
		}
		else
		{
			receivedData.erase(receivedData.begin(), receivedData.begin() + pos);
		}

		//call the base function once it has been prepared
		ObsList obsList = ObsStream::getObs();
		return obsList;
	}
	
	void getNav()
	{
		//get all data available from the ntrip stream and convert it to an iostream for processing
		getData();

		B_io::basic_array_source<char>					input_source(receivedData.data(), receivedData.size());
		B_io::stream<B_io::basic_array_source<char>>	inputStream(input_source);

		parseRTCM(inputStream);

		int pos = inputStream.tellg();
		if (pos < 0)
		{
			receivedData.clear();
		}
		else
		{
			receivedData.erase(receivedData.begin(), receivedData.begin() + pos);
		}
	}
};

/** Object that streams RTCM data from a file.
 * Overrides interface functions
 */
struct FileRtcmStream : ACSFileStream, RtcmStream
{
	FileRtcmStream(const string& path)
	{		
		sourceString = path;
		setPath(path);
		open();
	}

	void open()
	{
		openFile();
	}

	int lastObsListSize = -1;

	ObsList getObs() override
	{
// 		getData();	not needed for files
		FileState fileState = openFile();

		if (obsListList.size() < 3)
		{
			parseRTCM(fileState.inputStream);
		}

		//call the base function once it has been prepared
		ObsList obsList = ObsStream::getObs();

		lastObsListSize = obsList.size();
		return obsList;
	}

	bool isDead() override
	{
		if (filePos < 0)
		{
			return true;
		}
		
		FileState fileState = openFile();
		
		if	( (lastObsListSize != 0)
			||(fileState.inputStream))
		{
			return false;
		}
		else
		{
			return true;
		}
	}
};

/** Object that streams RINEX data from a file.
 * Overrides interface functions
 */
struct FileRinexStream : ACSFileStream, RinexStream
{
	FileRinexStream()
	{

	}

	FileRinexStream(const string& path)
	{		
		sourceString = path;
		setPath(path);
		open();
	}

	void open()
	{
		FileState fileState = openFile();
		
		readRinexHeader(fileState.inputStream);
	}

	int lastObsListSize = -1;

	ObsList getObs() override
	{
		FileState fileState = openFile();
		
// 		getData();	not needed for files

		if (obsListList.size() < 2)
		{
			parseRINEX(fileState.inputStream);
		}

		//call the base function once it has been prepared
		ObsList obsList = ObsStream::getObs();

		lastObsListSize = obsList.size();
		return obsList;
	}
	
	bool parse()
	{
		FileState fileState = openFile();
		
		parseRINEX(fileState.inputStream);
		
		return true;
	}

	bool isDead() override
	{
		if (filePos < 0)
		{
			return true;
		}
		
		FileState fileState = openFile();
		
		if	( lastObsListSize != 0
			||fileState.inputStream)
		{
			return false;
		}
		else
		{
			return true;
		}
	}
};


typedef std::shared_ptr<ObsStream> ACSObsStreamPtr;
typedef std::shared_ptr<NavStream> ACSNavStreamPtr;

#endif
