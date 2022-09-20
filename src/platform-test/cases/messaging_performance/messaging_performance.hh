
#ifndef PLATFORM_TEST_CASES_MESSAGING_PERFORMANCE_MESSAGING_PERFORMANCE_HH
#define PLATFORM_TEST_CASES_MESSAGING_PERFORMANCE_MESSAGING_PERFORMANCE_HH

#include <framework/test_case.hh>

class TestMessagingPerformance : public TestCase::Instance {
public:
    TestMessagingPerformance(const char * name) : TestCase::Instance(name) {}
    virtual u32 GetParamsSize(void) override;
    virtual int ParseParams(char * paramsIn, void * paramsOut) override;
    virtual int StartTest(void * args) override;
    virtual void StopTest(void) override;

private:
    int StartTriggerTimer(u64 period);
};

#endif /* PLATFORM_TEST_CASES_MESSAGING_PERFORMANCE_MESSAGING_PERFORMANCE_HH */
