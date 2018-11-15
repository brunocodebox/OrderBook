#pragma once

#include <iostream>
#include <string>
#include <exception>
#include <boost/thread.hpp>

using namespace std;

// Define an error structure that allow enough details to be passed to the end users and developers.
// This structure can be extended to include additional details to catch exceptions at deeper levels.
struct ErrorExceptionInfo {
	string				szDesc;
	string				szReason;
	string				szFunc;
	boost::thread::id	tid;
};

class TracedException : public std::exception {

public:
	// Overload the exception what() function
	virtual const char* what() const throw() { return m_eei.szDesc.c_str(); }

	// Make a traced exception with compulsory message
	TracedException() = delete;

	TracedException(const string& szDesc, const string& szReason, const string& szFunc) {
		setExceptionInfo(szDesc, szReason, szFunc);
	}

	TracedException(const ErrorExceptionInfo& eei) {
		// Preserve thread id
		m_eei = eei;
	}

	// There is no internal memory allocated so no need to provide destructor
	// virtual ~TracedException() throw() {}

	void setExceptionInfo(const string& szDesc, const string& szReason, const string& szFunc) noexcept {
		m_eei.szDesc = szDesc;
		m_eei.szReason = szReason;
		m_eei.szFunc = szFunc;
		m_eei.tid = boost::this_thread::get_id();
	}

	// Getting exception ingo will not throw errors
	const ErrorExceptionInfo& getExceptionInfo() const noexcept {
		return m_eei;
	}

	// Output exception info without throwing exception
	const void coutException() const noexcept {
		cout << "Description:" << m_eei.szDesc.c_str() << ", Reason:" << m_eei.szReason << ", Function:" << m_eei.szFunc << ", Thread id:" << m_eei.tid << endl;
	}

private:
	ErrorExceptionInfo	m_eei;

public:
	static constexpr auto SZ_EXCEPTION_BADALLOC = "Allocation failed(bad_alloc)";
	static constexpr auto SZ_EXCEPTION_UNEXPECTED = "Caught unexpected exception";
};