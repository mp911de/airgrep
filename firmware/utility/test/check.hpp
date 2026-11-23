#pragma once

#include <cstdio>
#include <exception>
#include <initializer_list>
#include <iostream>
#include <type_traits>

namespace check {
inline int checks = 0, failures = 0;
inline const char* running = "";

inline void record(bool passed, const char* file, int line, const char* what) {
	checks++;
	if (!passed) {
		failures++;
		std::printf("%s:%d: FAIL in %s: %s\n", file, line, running, what);
	}
}

// A small integer prints as a number, not as a character.
template <typename Value>
decltype(auto) printable(const Value& value) {
	if constexpr (std::is_integral_v<Value>) {
		return +value;
	} else {
		return (value);
	}
}

template <typename Expected, typename Actual>
void equal(const Expected& expected, const Actual& actual, const char* file,
		   int line, const char* what) {
	const bool passed = expected == actual;
	record(passed, file, line, what);
	if (!passed) {
		std::cout << "  expected " << printable(expected) << ", got "
				  << printable(actual) << "\n";
	}
}

struct Test {
	const char* name;
	void (*run)();
};

inline int run(std::initializer_list<Test> tests) {
	for (const Test& test : tests) {
		running = test.name;
		try {
			test.run();
		} catch (const std::exception& error) {
			failures++;
			std::printf("FAIL in %s: it threw %s\n", test.name, error.what());
		}
	}
	std::printf("%d checks, %d failures\n", checks, failures);
	return failures ? 1 : 0;
}
}

#define CHECK(condition) \
	check::record((condition), __FILE__, __LINE__, #condition)
#define CHECK_EQUAL(expected, actual) \
	check::equal((expected), (actual), __FILE__, __LINE__, #actual)
#define TEST(name) \
	check::Test { #name, name }
