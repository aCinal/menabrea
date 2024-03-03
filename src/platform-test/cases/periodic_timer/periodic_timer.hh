
#ifndef PLATFORM_TEST_CASES_PERIODIC_TIMER_PERIODIC_TIMER_HH
#define PLATFORM_TEST_CASES_PERIODIC_TIMER_PERIODIC_TIMER_HH

#include <menabrea/test/test_case.hh>

class TestPeriodicTimer : public TestCase::Instance {
public:
    TestPeriodicTimer(const char * name) : TestCase::Instance(name) {}
    virtual u32 GetParamsSize(void) override;
    virtual int ParseParams(char * paramsIn, void * paramsOut) override;
    virtual int StartTest(void * args) override;
    virtual void StopTest(void) override;
};

#endif /* PLATFORM_TEST_CASES_PERIODIC_TIMER_PERIODIC_TIMER_HH */
