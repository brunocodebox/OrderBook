#pragma once

#include "TracedException.hpp"
#include "OrderBook.hpp"

class OBStream
{
protected:
	// OrderBook used for plotting
	boost::shared_ptr<OrderBook>	m_pOrderBook;

	// Trace any exception that might occur
	ErrorExceptionInfo m_eei;

public:
	OBStream(const string& szFile, const int& nMaxBookLevels);

	const string& getSourceFeed() const					{ return m_pOrderBook->szSourceFeed; }
	int getNumFeeds() const								{ return m_pOrderBook->nBookFeeds; }

	operator boost::shared_ptr<OrderBook>()				{ return m_pOrderBook; }
	boost::shared_ptr<OrderBook> getOrderBook()			{ return m_pOrderBook; }

	const bool IsCaughtException() const				{ return !m_eei.szDesc.empty(); }
	void setExceptionInfo(const TracedException& te)	{ m_eei = te.getExceptionInfo(); }

	virtual void CheckNotifyException() const;

	virtual void processFeeds()					= 0;
	virtual const string getObjectName() const	= 0;

protected:
	int  addLevels(vecLevels&, const string& szLevel, const regex& re, vecPairInt& vps);
	void processLevel(const string& szBidLevel, const string& szAskLevel, const regex& reg);

	//void setDiffLevels();

private:
	static constexpr auto SZ_OBSTREAM_EXCEPTION = "OBStream Exception";
};

class OBStreamCSV : public OBStream {
public:
	OBStreamCSV() = delete;
	OBStreamCSV(const string& szFile, int& nMaxBookLevels) : OBStream(szFile, nMaxBookLevels) {}

	void processFeeds();
	const string getObjectName() const { return "OBStreamCSV"; }

	enum CSVFEED_ROW_ID {
		CSVFEED_INSTRUMENT = 0,
		CSVFEED_DATETIME,
		CSVFEED_FLAGS,
		CSVFEED_VOLUMEACC,
		CSVFEED_STATUS,
		CSVFEED_LATEST_PRICE,
		CSVFEED_LATEST_SIZE,
		CSVFEED_ASK_PRICE = 7,
		CSVFEED_ASK_SIZE,
		CSVFEED_BID_PRICE,
		CSVFEED_BID_SIZE,
		CSVFEED_BID_LEVELS,
		CSVFEED_ASK_LEVELS
	};

private:
	static constexpr auto SZ_OBSTREAMCSV_EXCEPTION = "OBStreamCSV Exception";
};

class OBStreamLog : public OBStream {
public:
	OBStreamLog() = delete;
	OBStreamLog(const string& szFile, int& nMaxBookLevels) : OBStream(szFile, nMaxBookLevels) {}

	void processFeeds();
	const string getObjectName() const { return "OBStreamLog"; }

	enum LOGFEED_ROW_ID {
		LOGFEED_INSTRUMENT = 0,
		LOGFEED_STATUS,
		LOGFEED_DATA_QUALITY,
		LOGFEED_BID_PRICESIZE,
		LOGFEED_ASK_PRICESIZE,
		LOGFEED_BID_BOOK,
		LOGFEED_ASK_BOOK
	};

private:
	static constexpr auto SZ_OBSTREAMLOG_EXCEPTION = "OBStreamLog Exception";
};



