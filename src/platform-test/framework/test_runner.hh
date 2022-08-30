
#ifndef PLATFORM_TEST_FRAMEWORK_TEST_RUNNER_HH
#define PLATFORM_TEST_FRAMEWORK_TEST_RUNNER_HH

#include <framework/test_case.hh>

namespace TestRunner {

void Init(void);
void Teardown(void);
void OnPollIn(char * inputString);
void OnPollHup(void);
void ReportTestResult(TestCase::Result result);
void ReportTestResult(TestCase::Result result, const char * extra, ...) __attribute__((format(printf, 2, 3)));

}

#endif /* PLATFORM_TEST_FRAMEWORK_TEST_RUNNER_HH */

