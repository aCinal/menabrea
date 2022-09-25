
#ifndef PLATFORM_TEST_FRAMEWORK_TEST_RUNNER_HH
#define PLATFORM_TEST_FRAMEWORK_TEST_RUNNER_HH

#include <framework/test_case.hh>

namespace TestRunner {

constexpr const u64 DEFAULT_TEST_TIMEOUT = 5 * 1000 * 1000;  /* 5 seconds */

void Init(void);
void Teardown(void);
void OnPollIn(char * inputString);
void OnPollHup(void);
void ReportTestResult(TestCase::Result result, const char * extra = " ", ...) __attribute__((format(printf, 2, 3)));
void ExtendTimeout(u64 remainingTime = DEFAULT_TEST_TIMEOUT);

}

#endif /* PLATFORM_TEST_FRAMEWORK_TEST_RUNNER_HH */

