#pragma once

#include <cstdint>
#include <cstdio>
#include <exception>
#include <sstream>
#include <string>

void setUp();
void tearDown();

namespace unity {

struct Failure : public std::exception {};

inline int testsRun = 0;
inline int testsFailed = 0;

template <typename T>
std::string toString(const T& value) {
  std::ostringstream stream;
  stream << value;
  return stream.str();
}

[[noreturn]] inline void fail(const char* file, const int line, const std::string& message) {
  ++testsFailed;
  std::fprintf(stderr, "FAIL %s:%d: %s\n", file, line, message.c_str());
  throw Failure{};
}

template <typename Expected, typename Actual>
void assertEqual(const Expected& expected,
                 const Actual& actual,
                 const char* file,
                 const int line,
                 const char* expectedExpr,
                 const char* actualExpr) {
  if (!(expected == actual)) {
    fail(file,
         line,
         std::string("expected ") + expectedExpr + " == " + actualExpr + ", got " + toString(expected) + " vs " +
             toString(actual));
  }
}

inline void assertTrue(const bool condition, const char* file, const int line, const char* expr) {
  if (!condition) {
    fail(file, line, std::string("assertion failed: ") + expr);
  }
}

template <typename Threshold, typename Actual>
void assertGreaterThan(const Threshold& threshold,
                       const Actual& actual,
                       const char* file,
                       const int line,
                       const char* thresholdExpr,
                       const char* actualExpr) {
  if (!(actual > threshold)) {
    fail(file,
         line,
         std::string("expected ") + actualExpr + " > " + thresholdExpr + ", got " + toString(actual) + " <= " +
             toString(threshold));
  }
}

template <typename TestFunc>
void runTest(TestFunc test, const char* name) {
  ++testsRun;
  try {
    setUp();
    test();
    tearDown();
    std::fprintf(stdout, "PASS %s\n", name);
  } catch (const Failure&) {
    tearDown();
  }
}

inline int begin() {
  testsRun = 0;
  testsFailed = 0;
  return 0;
}

inline int end() {
  std::fprintf(stdout, "Tests run: %d, failures: %d\n", testsRun, testsFailed);
  return testsFailed == 0 ? 0 : 1;
}

}  // namespace unity

#define UNITY_BEGIN() unity::begin()
#define UNITY_END() unity::end()
#define RUN_TEST(func) unity::runTest(func, #func)

#define TEST_ASSERT_TRUE(condition) unity::assertTrue((condition), __FILE__, __LINE__, #condition)
#define TEST_ASSERT_NOT_NULL(pointer) unity::assertTrue(((pointer) != nullptr), __FILE__, __LINE__, #pointer " != nullptr")
#define TEST_ASSERT_EQUAL(expected, actual) \
  unity::assertEqual((expected), (actual), __FILE__, __LINE__, #expected, #actual)
#define TEST_ASSERT_EQUAL_UINT32(expected, actual) \
  unity::assertEqual<uint32_t, uint32_t>((expected), (actual), __FILE__, __LINE__, #expected, #actual)
#define TEST_ASSERT_GREATER_THAN_UINT32(threshold, actual) \
  unity::assertGreaterThan<uint32_t, uint32_t>((threshold), (actual), __FILE__, __LINE__, #threshold, #actual)
