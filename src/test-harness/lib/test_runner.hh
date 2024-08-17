
#ifndef TEST_HARNESS_TEST_RUNNER_HH
#define TEST_HARNESS_TEST_RUNNER_HH

#include <menabrea/test/test_case.hh>
#include <stdarg.h>

namespace TestRunner {

void Init(void);
void Teardown(void);
void OnPollIn(char * inputString);
void OnPollHup(void);
void ReportTestResult(TestCase::Result result, const char * extra, va_list args);
void ExtendTimeout(u64 remainingTime);

}

#endif /* TEST_HARNESS_TEST_RUNNER_HH */
