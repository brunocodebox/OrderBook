#pragma once

typedef pair<int, int>				pairInt;
typedef vector<pairInt>				vecPairInt;

typedef struct BidAskLevels {
	vecPairInt	vBidQty;
	vecPairInt	vAskQty;
} BidAskLevels;

typedef set<int>					setInt;
typedef map<int, setInt>			mapPriceQty;
typedef vector<mapPriceQty>			vecLevels;

typedef map<int, BidAskLevels>		mapBidAskLevels;
typedef map<int, mapBidAskLevels>	mapSpreadLevels;

const int MAX_BOOK_LEVELS = 5;

struct OrderBook
{
	string			szSourceFeed;		// Files with bid/ask feeds

	int				nBookFeeds;			// Number of book feeds
	int				nBookLevels;		// Maximum user required book levels

	vector<int>		vecBidTotal;		// Run up total of bid feeds at each level
	vector<int>		vecAskTotal;		// Run up total of ask feeds at each level

	mapSpreadLevels	mapBestSpread;		// Best inside market spreads

	vecLevels		vecBidLevels;		// Summary of market bid levels
	vecLevels		vecAskLevels;		// Summary of market ask levels
};
