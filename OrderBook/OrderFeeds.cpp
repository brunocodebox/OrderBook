//==============================================================
// Copyright Bruno Kieba - 2018
// 
// Updated main with main(argc, argv)
// Added XML file to select source feed files
// Added thread synchronization
//==============================================================
#include "pch.h"
#include <iostream>
#include <vector>
#include <fstream>
#include <boost/regex.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/range/irange.hpp>
#include <boost/format.hpp>
#include <boost/thread.hpp>
#include <boost/asio/thread_pool.hpp>
#include <boost/foreach.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/xml_parser.hpp>

using namespace std;
using namespace boost;

#include "OrderFeeds.hpp"
#include "OrderPlot.hpp"

OBStream::OBStream(const string& szFile, const int& nMaxBookLevels) {

	// Stub to allocate function name at compile time
	static const string SZ_OBSTREAM_CONSTRUCTOR = "OBStream::OBStream";

	try {
		m_pOrderBook = boost::make_shared<OrderBook>();
		m_pOrderBook->szSourceFeed = szFile;

		// Initialize user requested levels
		m_pOrderBook->nBookLevels = nMaxBookLevels;

		// Resize the vectors counting the number of bid and ask feeds at each level
		m_pOrderBook->vecBidTotal.resize(nMaxBookLevels);
		m_pOrderBook->vecAskTotal.resize(nMaxBookLevels);
	}

	catch (const std::bad_alloc&) {
		TracedException te(SZ_OBSTREAM_EXCEPTION, TracedException::SZ_EXCEPTION_BADALLOC, SZ_OBSTREAM_CONSTRUCTOR);
		throw te;
	}
	catch (...) {
		TracedException te(SZ_OBSTREAM_EXCEPTION, TracedException::SZ_EXCEPTION_UNEXPECTED, SZ_OBSTREAM_CONSTRUCTOR);
		throw te;
	}
}

using boost::lexical_cast;
using boost::bad_lexical_cast;

int OBStream::addLevels(vecLevels& vLevels, const string& szLevel, const regex& re, vecPairInt& vps) {

	// Stub to allocate function name at compile time
	static const string SZ_OBSTREAM_ADDLEVELS = "addLevels";
	
	// Initialize the level processed so far
	int iLevel = 0;

	try {
		for (sregex_iterator rit = sregex_iterator(szLevel.begin(), szLevel.end(), re); rit != sregex_iterator(); ++rit, ++iLevel)
		{
			// Read only the user configured levels
			if (iLevel == m_pOrderBook->nBookLevels)
				break;

			// Read regex match and extract price and quantity
			smatch m = *rit;
			int nPrice = boost::lexical_cast<int>(m.str(1));
			int nQty   = boost::lexical_cast<int>(m.str(2));

			// Add the quantity of this price in its quantity set
			try {
				mapPriceQty& mpq = vLevels.at(iLevel);

				// Insert quantity to quantity set and quantity set to map using [] operator
				mpq[nPrice].insert(nQty);
			}
			catch (const std::out_of_range) {

				// This is a new level therefore a new level with price and quantities must be added
				setInt qtyset;
				qtyset.insert(nQty);

				// Insert quantity set to map with [] operator
				mapPriceQty mpq;
				mpq[nPrice] = qtyset;

				// Save this next level
				vLevels.push_back(mpq);
			}
			catch (...) {
				TracedException te(SZ_OBSTREAM_EXCEPTION, TracedException::SZ_EXCEPTION_UNEXPECTED, SZ_OBSTREAM_ADDLEVELS);
				throw te;
			}

			// Insert scanned prices lowest to largest
			vps.push_back(make_pair(nPrice, nQty));
		}
	}
	catch (const std::bad_alloc&) {
		TracedException te(SZ_OBSTREAM_EXCEPTION, TracedException::SZ_EXCEPTION_BADALLOC, SZ_OBSTREAM_ADDLEVELS);
		throw te;
	}
	catch (...) {
		TracedException te(SZ_OBSTREAM_EXCEPTION, TracedException::SZ_EXCEPTION_UNEXPECTED, SZ_OBSTREAM_ADDLEVELS);
		throw te;
	}

	// Return number of levels processed
	return iLevel;
}

void OBStream::processLevel(const string& szBidLevel, const string& szAskLevel, const regex& reg) {

	BidAskLevels bal;

	int nBidLevels = addLevels(m_pOrderBook->vecBidLevels, szBidLevel, reg, bal.vBidQty);
	int nAskLevels = addLevels(m_pOrderBook->vecAskLevels, szAskLevel, reg, bal.vAskQty);

	// Update the number of feeds
	m_pOrderBook->nBookFeeds++;

	// Make sure the bid ask feeds are valid
	if (nBidLevels == 0 || nAskLevels == 0)
		return;

	// Update the number of bid and ask feeds at each level
	for (auto itb = std::begin(m_pOrderBook->vecBidTotal); itb != std::end(m_pOrderBook->vecBidTotal) && boost::distance(std::begin(m_pOrderBook->vecBidTotal), itb) < nBidLevels; ++itb) { ++(*itb); }
	for (auto ita = std::begin(m_pOrderBook->vecAskTotal); ita != std::end(m_pOrderBook->vecAskTotal) && boost::distance(std::begin(m_pOrderBook->vecAskTotal), ita) < nAskLevels; ++ita) { ++(*ita); }

	// Log the inside market spread and bid ask price and size on this feed
	const pairInt& pbs = bal.vBidQty.at(0);	// fetch first bid pair
	const pairInt& pas = bal.vAskQty.at(0);	// fetch first ask pair

	// Log spread key, bid price key, and levels
	m_pOrderBook->mapBestSpread[pas.first - pbs.first][pbs.first] = bal;	// calculate spread and log it with associated bid and ask
}

void OBStream::CheckNotifyException() const {

	if (IsCaughtException())
	{
		// Identify which class object caught an exception
		cout << "Exception was caught processing feeds in class object " << getObjectName() << endl;

		// Rethrow the caught exception up the call stack
		TracedException te(m_eei);
		throw te;
	}
}

void OBStreamCSV::processFeeds()
{
	regex reQuotedFields("\"(.*?)\"");
	regex rePriceQty("Price:\\s+([0-9]+)\\s+Quantity:\\s+([0-9]+)");

	ifstream file(getSourceFeed());
	string line;
	
	// Stub to allocate function name at compile time
	static const string SZ_OBSTREAMCSV_PROCESSFEEDS = "processFeeds";

	// Skip header line
	getline(file, line);

	// Safely process all the csv feeds to build the order book
	try {
		// Read and parse all feeds
		while (getline(file, line)) {

			// Split each feed line into separate words such as Instrument, date, status, etc.
			vector<string> fields;

			// Use a Regex expression to identify the beginning and end of each word and push it in a vector
			for (sregex_iterator i = sregex_iterator(line.begin(), line.end(), reQuotedFields); i != sregex_iterator(); ++i)
			{
				smatch m = *i;
				string s = m.str(1);

				// Push words from feed row safely
				fields.push_back(s);
			}

			//int nBidLevels = addLevels(m_pOrderBook->vecBidLevels, fields.at(OBStreamCSV::CSVFEED_BID_LEVELS), rePriceQty);
			//int nAskLevels = addLevels(m_pOrderBook->vecAskLevels, fields.at(OBStreamCSV::CSVFEED_ASK_LEVELS), rePriceQty);

			// Update the line feeds and increment the count of feed for each level
			processLevel(fields.at(OBStreamCSV::CSVFEED_BID_LEVELS), fields.at(OBStreamCSV::CSVFEED_ASK_LEVELS), rePriceQty);
		}

		// Close the file
		file.close();
	}
	catch (const TracedException& te) {
		setExceptionInfo(te);
	}
	catch (const std::bad_alloc&) {
		TracedException te(SZ_OBSTREAMCSV_EXCEPTION, TracedException::SZ_EXCEPTION_BADALLOC, SZ_OBSTREAMCSV_PROCESSFEEDS);
		setExceptionInfo(te);
	}
	catch (...) {
		// Log unexpected exception and let caller decide what to do
		TracedException te(SZ_OBSTREAMCSV_EXCEPTION, TracedException::SZ_EXCEPTION_UNEXPECTED, SZ_OBSTREAMCSV_PROCESSFEEDS);
		setExceptionInfo(te);
	}
}

void OBStreamLog::processFeeds()
{
	regex reEllipsis("\\{(.*?)\\}");
	regex rePriceQty("([0-9]*),([0-9]*)");

	ifstream file(getSourceFeed());
	string line;

	// Stub to allocate function name at compile time
	static const string SZ_OBSTREAMLOG_PROCESSFEEDS = "processFeeds";

	// Safely process all the log feeds to build the order book
	try {
		// Read and parse all row feeds
		while (getline(file, line)) {

			vector<string> fields;

			// Use a Regex expression to identify the beginning and end of each value field within ellipsis and push it in a vector
			for (sregex_iterator i = sregex_iterator(line.begin(), line.end(), reEllipsis); i != sregex_iterator(); ++i)
			{
				smatch m = *i;
				string s = m.str(1);
				fields.push_back(s);
			}

			// Use a Regex expression to build the bid and ask levels on this feed line
			//addLevels(m_pOrderBook->vecBidLevels, fields.at(OBStreamLog::LOGFEED_BID_BOOK), rePriceQty);
			//addLevels(m_pOrderBook->vecAskLevels, fields.at(OBStreamLog::LOGFEED_ASK_BOOK), rePriceQty);

			// Count this feed
			processLevel(fields.at(OBStreamLog::LOGFEED_BID_BOOK), fields.at(OBStreamLog::LOGFEED_ASK_BOOK), rePriceQty);
		}

		// Close the file
		file.close();
	}
	catch (const TracedException& te) {
		setExceptionInfo(te);
	}
	catch (const std::bad_alloc&) {
		TracedException te(SZ_OBSTREAMLOG_EXCEPTION, TracedException::SZ_EXCEPTION_BADALLOC, SZ_OBSTREAMLOG_PROCESSFEEDS);
		setExceptionInfo(te);
	}
	catch (...) {
		// Log unexpected exception and let caller decide what to do
		TracedException te(SZ_OBSTREAMLOG_EXCEPTION, TracedException::SZ_EXCEPTION_UNEXPECTED, SZ_OBSTREAMLOG_PROCESSFEEDS);
		setExceptionInfo(te);
	}
}
