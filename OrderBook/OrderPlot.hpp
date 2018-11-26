#pragma once

const int MAX_BEST_SPREADS = 5;

typedef struct InjectParams {

	string		szHtml;
	string		szHeader;
	string		szTitle;
	string		szSubTitle;

	string		szMarkerBegin;
	string		szMarkerEnd;

	int			nMarketLevels;
	int			nParam;
	string		szParam;

} InjectParams;


// Base class
class OrderPlot {

public:
	// Chart plotting interface methds
	OrderPlot() = delete;
	explicit OrderPlot(const string& szXmlFile, OBStreamCSV& obsCsv, OBStreamLog& obsLog);
	const string& getPlotFile() const { return m_szPlotFile; }

private:
	void	plotBookSummary(InjectParams& ijParams);
	void	plotBookCol(boost::shared_ptr<OrderBook>& m_pBook, InjectParams& ijParams, stringstream& ss);
	void	plotBookLevelsDiff(vecLevels& vCsvLevels, vecLevels& vLogLevels, InjectParams& ijParams, stringstream& ss);
	void	plotLevels(vecLevels& vl, InjectParams& ijParams, stringstream& ss);
	void	plotLevelCol(const vecPairInt& vpi, InjectParams& ijParams, stringstream& ss, bool bFluid=true);
	void	injectHtml(const InjectParams& ijParams, const stringstream& ss);

private:
	string	m_szPlotFile;
	boost::shared_ptr<OrderBook> m_pCsvBook;
	boost::shared_ptr<OrderBook> m_pLogBook;
};
