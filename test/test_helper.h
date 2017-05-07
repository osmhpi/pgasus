#pragma once

#include <exception>
#include <iostream>
#include <string>


namespace testing
{

class TestFailure : public std::exception
{
public:
	TestFailure(const std::string & what = {})
		: m_what{ what }
	{
	}
	~TestFailure() override = default;

#if defined(_GLIBCXX_TXN_SAFE_DYN) || defined(_GLIBCXX_USE_NOEXCEPT)
#define EXCEPTION_WHAT_NOEXCEPT _GLIBCXX_TXN_SAFE_DYN _GLIBCXX_USE_NOEXCEPT
#else
#define EXCEPTION_WHAT_NOEXCEPT nothrow
#endif
	const char* what() const EXCEPTION_WHAT_NOEXCEPT override
	{
		return m_what.c_str();
	}

private:
	std::string m_what;
};

inline void initialize()
{
	std::set_terminate([] () {
		try {
			if (auto unknown = std::current_exception()) {
				std::rethrow_exception(unknown);
			}
		}
		catch (const TestFailure & testFailure) {
			std::cerr << testFailure.what() << std::endl;
		}
		catch (const std::exception & e) {
			std::cerr << "Exception thrown during test execution: \""
				<< e.what() << "\"" << std::endl;
		}
		catch (...) {
			std::cerr << "Unkown exception thrown during test execution."
				<< std::endl;
		}
	});
}

}

/**
 * ASSERT_TRUE test macro that is meant to be replaceable by google test macros.
 */
#define ASSERT_TRUE(X) \
	if (!(X)) {\
		const std::string msg = "Test failed in " + std::string(__FILE__) \
			+ ":" + std::to_string(__LINE__) + " - " + std::string(__func__) \
			+ "\n\tTest expression: " + std::string(#X); \
		throw testing::TestFailure(msg); \
	}
