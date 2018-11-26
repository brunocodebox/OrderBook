//==============================================================
// Copyright Bruno Kieba - 2018
// 
// Plot Updated main with main(argc, argv)
// Added XML file to select source feed files
// Added thread synchronization
//==============================================================
#include "pch.h"
#include <iostream>
#include <set>
#include <map>
#include <vector>
#include <algorithm>
#include <fstream>
#include <boost/regex.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/foreach.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/range/adaptors.hpp>
#include <boost/range/adaptor/reversed.hpp>
#include <boost/range/irange.hpp>
#include <boost/range/numeric.hpp>
#include <boost/multiprecision/cpp_dec_float.hpp>
#include <boost/format.hpp>
#include <boost/shared_ptr.hpp>

#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/xml_parser.hpp>

using namespace std;
using namespace boost;

#include "OrderBook.hpp"
#include "OrderFeeds.hpp"
#include "OrderPlot.hpp"

using boost::lexical_cast;
using boost::bad_lexical_cast;

const string szBookPlot("task1.bookplot.");

OrderPlot::OrderPlot(const string& szXml, OBStreamCSV& obsCsv, OBStreamLog& obsLog) {

	// Mke sure there is data to work with
	m_pCsvBook = obsCsv.getOrderBook();
	m_pLogBook = obsLog.getOrderBook();

	assert(m_pCsvBook);
	assert(m_pLogBook);

	// Setup the tree to parse the xml file
	using namespace boost::property_tree::xml_parser;
	using boost::property_tree::ptree;
	ptree pt;
	read_xml(szXml, pt, trim_whitespace | no_comments);

	// Gt the name of the html file to generate
	m_szPlotFile = pt.get<string>(szBookPlot + "file", "orderbook.htm");

	InjectParams ijParams;
	ijParams.szHtml = m_szPlotFile;

	// Plot the csv and log book orders summary results
	ijParams.szHeader = pt.get<string>(szBookPlot + "summary", "Order Books Summary");
	ijParams.szMarkerBegin = pt.get<string>(szBookPlot + "markers.begin_summary", "begin summary");
	ijParams.szMarkerEnd = pt.get<string>(szBookPlot + "markers.end_summary", "end summary");
	ijParams.nParam = pt.get<int>(szBookPlot + "bestspreads", MAX_BEST_SPREADS);
	plotBookSummary(ijParams);
}

void OrderPlot::plotBookSummary(InjectParams& ijParams) {

	stringstream ss;

	// Build two-column summary to see results side-by-side

	// - Source feeds
	ss << "\t<div class='container-fluid'>" << endl;
	ss << "\t<h3 class='linebot'>" << ijParams.szHeader << "</h3>" << endl;
	ss << "\t\t<div class='row'>" << endl;

	plotBookCol(m_pCsvBook, ijParams, ss);
	plotBookCol(m_pLogBook, ijParams, ss);

	ss << "\t\t</div>" << endl;
	ss << "\t</div>" << endl;

	// Price-Quantity offers' differences
	ss << "\t<h3 class='gapsep'>Price-Quantity offers:</h3>" << endl;
	ss << "\t\t\t\t<h5 class='gapsep'>Bid price offers:</h5>" << endl;

	// Plot bid results first
	ijParams.szParam = "Bid";
	plotBookLevelsDiff(m_pCsvBook->vecBidLevels ,m_pLogBook->vecBidLevels, ijParams, ss);

	// Then ask results below
	ss << "\t\t\t\t<h5 class='gapsep'>Ask price offers:</h5>" << endl;
	ijParams.szParam = "Ask";
	plotBookLevelsDiff(m_pCsvBook->vecAskLevels, m_pLogBook->vecAskLevels, ijParams, ss);

	// Update the html file
	injectHtml(ijParams, ss);
}

void OrderPlot::plotBookLevelsDiff(vecLevels& vCsvLevels, vecLevels& vLogLevels, InjectParams& ijParams, stringstream& ss) {

	// Scan through the union of csv and log levels
	int nMaxLevels = max(vCsvLevels.size(), vLogLevels.size());

	for (size_t i : boost::irange(0, nMaxLevels)) {

		// Container of price quantity difference
		vecPairInt vpiCsv, vpiLog;

		// Use try-catch block to catch unequal levels between source feeds
		try {
			mapPriceQty& m1 = vCsvLevels.at(i);
			mapPriceQty& m2 = vLogLevels.at(i);

			setInt keys1, keys2, interKeys, diffKeys1, diffKeys2;

			// Extract the keys from each map with a transform
			std::transform(m1.begin(), m1.end(), std::inserter(keys1, keys1.begin()), [](mapPriceQty::value_type &m) { return m.first; });
			std::transform(m2.begin(), m2.end(), std::inserter(keys2, keys2.begin()), [](mapPriceQty::value_type &m) { return m.first; });

			// Filter out keys in keys1 not in keys2
			std::set_difference(keys1.begin(), keys1.end(), keys2.begin(), keys2.end(), std::inserter(diffKeys1, diffKeys1.begin()));

			// Filter out keys in keys2 not in keys1
			std::set_difference(keys2.begin(), keys2.end(), keys1.begin(), keys1.end(), std::inserter(diffKeys2, diffKeys2.begin()));

			// Make pairs of different prices
			for (auto& k1 : diffKeys1) {

				vecPairInt vpi;

				// Invert the price in the pair so we know it is a price difference rather than a quantity. It will be deinverted later.
				std::transform(m1[k1].begin(), m1[k1].end(), std::back_inserter(vpi), [&k1](const int& q) { return std::make_pair(-1 * k1, q); });

				// Append price,quantity pairs difference found in csv feeds
				vpiCsv.insert(std::end(vpiCsv), std::begin(vpi), std::end(vpi));
			}

			// Make pairs of different prices
			for (auto& k2 : diffKeys2) {

				vecPairInt vpi;

				// Invert the price in the pair so we know it is a price difference rather than a quantity. It will be deinverted later.
				std::transform(m2[k2].begin(), m2[k2].end(), std::back_inserter(vpi), [&k2](const int& q) { return std::make_pair(-1 * k2, q); });

				// Append price,quantity pairs difference found in log feeds 
				vpiLog.insert(std::end(vpiLog), std::begin(vpi), std::end(vpi));
			}

			// Pick up the intersecting bid prices
			std::set_intersection(keys1.begin(), keys1.end(), keys2.begin(), keys2.end(), std::inserter(interKeys, interKeys.begin()));

			// And filter out prices with different quantity
			vecPairInt vpi1, vpi2;
			for (auto& k : interKeys) {
 
				setInt q1Diff, q2Diff;

				// Filter out the difference between the two set of quantities
				std::set_difference(m1[k].begin(), m1[k].end(), m2[k].begin(), m2[k].end(), std::inserter(q1Diff, q1Diff.begin()));
				std::set_difference(m2[k].begin(), m2[k].end(), m1[k].begin(), m1[k].end(), std::inserter(q2Diff, q2Diff.begin()));

				vecPairInt vpi1, vpi2;
				std::transform(q1Diff.begin(), q1Diff.end(), std::back_inserter(vpi1), [&k](const int& q) { return std::make_pair(k, q); });
				std::transform(q2Diff.begin(), q2Diff.end(), std::back_inserter(vpi2), [&k](const int& q) { return std::make_pair(k, q); });

				// Append price,quantity pairs difference between csv and log feeds 
				vpiCsv.insert(std::end(vpiCsv), std::begin(vpi1), std::end(vpi1));
				vpiLog.insert(std::end(vpiLog), std::begin(vpi2), std::end(vpi2));
			}

			// Plot the csv and log differences
			ss << "\t<div class='container-fluid'>" << endl;
			ss << "\t\t<h5 class='linebot'>Level " << i+1 << "</h5>" << endl;
			ss << "\t\t<div class='row'>" << endl;
			plotLevelCol(vpiCsv, ijParams, ss);
			plotLevelCol(vpiLog, ijParams, ss);
			ss << "\t\t</div>" << endl;
			ss << "\t</div>" << endl;
		}
		catch (...) {
			// PLot single side levels either csv or log. One of the two won't plot because its level size is out-of-band compared to the other
			plotLevels(vCsvLevels, ijParams, ss);
			plotLevels(vLogLevels, ijParams, ss);
		}
	}
}

void OrderPlot::plotLevels(vecLevels& vl, InjectParams& ijParams, stringstream& ss) {

	//for (size_t i : boost::irange(0, vl.size()) {
	int iLevel = 0;
	for (auto& m : vl) {

		// Extract the key prices for this level
		setInt keys;
		std::transform(m.begin(), m.end(), std::inserter(keys, keys.begin()), [](const mapPriceQty::value_type &m) { return m.first; });

		// Running total of price quantity pairs
		vecPairInt vpiLevel;

		// Pair up each kay with associated quantities
		for (auto& k : keys) {

			vecPairInt vpi;
			std::transform(m[k].begin(), m[k].end(), std::inserter(vpi, vpi.begin()), [&k](const int& q) { return std::make_pair(k, q); });
			vpiLevel.insert(std::end(vpiLevel), std::begin(vpi), std::end(vpi));
		}

		// Plot the csv and log differences
		ss << "\t\t<h5 class='linebot'>Level " << ++iLevel << "</h5>" << endl;
		plotLevelCol(vpiLevel, ijParams, ss, false);
	}
}

void OrderPlot::plotLevelCol(const vecPairInt& vpi, InjectParams& ijParams, stringstream& ss, bool bFluid) {

	// Plot column for a single row or a fluid row with two columns
	if (bFluid) {
		ss << "\t\t\t<div class='col-sm-5'>" << endl;
	}

	// Plot section subtitle
	ss << "\t\t\t\t<div class='row'>" << endl;
	ss << "\t\t\t\t\t<div class='col-3'>" << ijParams.szParam << " differences:</div>" << endl;
	ss << "\t\t\t\t\t<div class='col-2'>" << vpi.size() << "</div>" << endl;
	ss << "\t\t\t\t</div>" << endl;

	// Plot price quantity pair
	for (auto& pi : vpi) {
		ss << "\t\t\t<div class='row'>" << endl;
		ss << "\t\t\t\t<div class='col-3'></div>" << endl;
		if (pi.first < 0)
			ss << "\t\t\t\t<div class='col-2'>{<span class='boldfield'>" << -1 * pi.first << "</span>," << pi.second << "}</div>" << endl;
		else
			ss << "\t\t\t\t<div class='col-2'>{" << pi.first << "," << "<span class='boldfield'>" << pi.second << "</span>}</div>" << endl;
		ss << "\t\t\t</div>" << endl;
	}

	if (bFluid) {
		ss << "\t\t\t</div>" << endl;
	}
}

void OrderPlot::plotBookCol(boost::shared_ptr<OrderBook>& pBook, InjectParams& ijParams, stringstream& ss) {

	// Plot the html to create a two-column summary for easy comparison
	ss << "\t\t\t<div class='col-sm-5'>" << endl;

	// Source feed name
	ss << "\t\t\t\t<div class='row'>" << endl;
	ss << "\t\t\t\t\t<div class='col-3'>Source feeds:</div>" << endl;
	ss << "\t\t\t\t\t<div class='col-2'>" << pBook->szSourceFeed << "</div>" << endl;
	ss << "\t\t\t\t</div>" << endl;

	// Total feeds read
	ss << "\t\t\t\t<div class='row'>" << endl;
	ss << "\t\t\t\t\t<div class='col-3'>Total feeds:</div>" << endl;
	ss << "\t\t\t\t\t<div class='col-2'>" << pBook->nBookFeeds << "</div>" << endl;
	ss << "\t\t\t\t</div>" << endl;

	// - Plot total bid levels
	ss << "\t\t\t\t<div class='row'>" << endl;
	ss << "\t\t\t\t\t<div class='col-3'>Bid price levels:</div>" << endl;
	ss << "\t\t\t\t\t<div class='col-2'>" << pBook->vecBidLevels.size() << "</div>" << endl;
	ss << "\t\t\t\t</div>" << endl;

	// Plot total ask levels
	ss << "\t\t\t\t<div class='row'>" << endl;
	ss << "\t\t\t\t\t<div class='col-3'>Ask price levels:</div>" << endl;
	ss << "\t\t\t\t\t<div class='col-2'>" << pBook->vecAskLevels.size() << "</div>" << endl;
	ss << "\t\t\t\t</div>" << endl;

	// Plot totals of bid and ask levels
	ss << "\t\t\t\t<div class='row'>" << endl;
	ss << "\t\t\t\t\t<div class='col-3'>Total price levels:</div>" << endl;
	ss << "\t\t\t\t\t<div class='col-2'>" << pBook->vecBidLevels.size() + pBook->vecAskLevels.size() << "</div>" << endl;
	ss << "\t\t\t\t</div>" << endl;

	// Plot best spread section
	ss << "\t\t\t\t<h3 class ='linesep'>Top best spreads:</h3>" << endl;

	try {

		// Keep a running total of plotted spread
		int nPlotSpreads = 0;

		// Plot best market spread
		for (const auto& ks : pBook->mapBestSpread) {

			if (nPlotSpreads++ == ijParams.nParam)
				break;

			ss << "\t\t\t\t<h5>Best " << nPlotSpreads << ":</h5>" << endl;

			// Plot the spread
			ss << "\t\t\t\t<div class='row bg-light'>" << endl;
			ss << "\t\t\t\t\t<div class='col-3'>Spread:</div>" << endl;
			ss << "\t\t\t\t\t<div class='col-2'>" << ks.first << "</div>" << endl;
			ss << "\t\t\t\t</div>" << endl;

			// Plot the midpoint
			// Add the spread to the bid price used as a key to the map of levels
			double dMidPoint = boost::lexical_cast<double>(ks.second.begin()->first + ks.first + ks.second.begin()->first) / 2.0;

			ss << "\t\t\t\t<div class='row bg-light'>" << endl;
			ss << "\t\t\t\t\t<div class='col-3'>Mid point:</div>" << endl;
			ss << "\t\t\t\t\t<div class='col-2'>" << dMidPoint << "</div>" << endl;;
			ss << "\t\t\t\t</div>" << endl;

			ss << "\t\t\t\t<div class='row bg-light gapsep'>" << endl;
			ss << "\t\t\t\t\t<div class='col'>Inside market:</div>" << endl;
			ss << "\t\t\t\t</div>" << endl;

			ss << "\t\t\t\t<div class='row bg-light gapsep'>" << endl;
			ss << "\t\t\t\t\t<div class='col'></div>" << endl;
			ss << "\t\t\t\t\t<div class='col linebot'></div>" << endl;
			ss << "\t\t\t\t\t<div class='col linebot'>Price</div>" << endl;
			ss << "\t\t\t\t\t<div class='col linebot'>Ask Size</div>" << endl;
			ss << "\t\t\t\t\t<div class='col'></div>" << endl;
			ss << "\t\t\t\t</div>" << endl;

			// Fetch the ask levels for this spread
			const BidAskLevels& balCsv = ks.second.begin()->second;

			for (const auto& as : boost::adaptors::reverse(balCsv.vAskQty)) {

				ss << "\t\t\t\t<div class='row bg-light'>" << endl;
				ss << "\t\t\t\t\t<div class='col'></div>" << endl;
				ss << "\t\t\t\t\t<div class='col'></div>" << endl;
				ss << "\t\t\t\t\t<div class='col'>" << as.first << "</div>" << endl;
				ss << "\t\t\t\t\t<div class='col'>" << as.second << "</div>" << endl;
				ss << "\t\t\t\t\t<div class='col'></div>" << endl;
				ss << "\t\t\t\t</div>" << endl;
			}

			ss << "\t\t\t\t<div class='row bg-light'>" << endl;
			ss << "\t\t\t\t\t<div class='col'></div>" << endl;
			ss << "\t\t\t\t\t<div class='col linebot'></div>" << endl;
			ss << "\t\t\t\t\t<div class='col linebot'></div>" << endl;
			ss << "\t\t\t\t\t<div class='col linebot'></div>" << endl;
			ss << "\t\t\t\t\t<div class='col'></div>" << endl;
			ss << "\t\t\t\t</div>" << endl;

			/* ss << "\t\t\t\t<div class='bg-light linesep'></div>" << endl; */

			for (const auto& bs : balCsv.vBidQty) {
				ss << "\t\t\t\t<div class='row bg-light'>" << endl;
				ss << "\t\t\t\t\t<div class='col'></div>" << endl;
				ss << "\t\t\t\t\t<div class='col'>" << bs.second << "</div>" << endl;
				ss << "\t\t\t\t\t<div class='col'>" << bs.first << "</div>" << endl;
				ss << "\t\t\t\t\t<div class='col'></div>" << endl;
				ss << "\t\t\t\t\t<div class='col'></div>" << endl;
				ss << "\t\t\t\t</div>" << endl;
			}

			ss << "\t\t\t\t<div class='row bg-light'>" << endl;
			ss << "\t\t\t\t\t<div class='col'></div>" << endl;
			ss << "\t\t\t\t\t<div class='col linetop'>Bid Size</div>" << endl;
			ss << "\t\t\t\t\t<div class='col linetop'>Price</div>" << endl;
			ss << "\t\t\t\t\t<div class='col linetop'></div>" << endl;
			ss << "\t\t\t\t\t<div class='col'></div>" << endl;
			ss << "\t\t\t\t</div>" << endl;
		}
	}
	catch (...) {
		// No need to do anything, it's under control.
	}
	ss << "\t\t\t</div>" << endl; 
}

void OrderPlot::injectHtml(const InjectParams& ijParams, const stringstream& ss) {

	// Now inject the built columns in the html
	vector<string> vHtml;

	ifstream file(ijParams.szHtml);
	string line;

	// First read and parse all the html lines
	while (getline(file, line)) {
		vHtml.push_back(line);
	}

	// We are done
	file.close();

	// Inject the lines between the begin and section
	ofstream ofs(ijParams.szHtml);
	regex reBegin(ijParams.szMarkerBegin);
	regex reEnd(ijParams.szMarkerEnd);
	smatch m;

	bool bSkipNextLines = false;

	// Merge trade bars
	for (auto& line : vHtml)
	{
		if (regex_search(line, m, reEnd) == true) {
			bSkipNextLines = false;
		}

		if (bSkipNextLines == true)
			continue;

		ofs << line << endl;

		if (regex_search(line, m, reBegin) == true) {

			// Inject summary html
			ofs << ss.str();

			// Skip the lines until the end marker line
			bSkipNextLines = true;
		}
	}
	ofs.close();
}


