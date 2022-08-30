
#ifndef PLATFORM_TEST_CASES_BASIC_TIMING_BASIC_TIMING_HH
#define PLATFORM_TEST_CASES_BASIC_TIMING_BASIC_TIMING_HH

#include <framework/test_case.hh>

class TestBasicTiming : public TestCase::Instance {
public:
    TestBasicTiming(const char * name) : TestCase::Instance(name) {}
    virtual u32 GetParamsSize(void) override;
    virtual int ParseParams(char * paramsIn, void * paramsOut) override;
    virtual int StartTest(void * args) override;
    virtual void StopTest(void) override;

private:
    int RunSubcase0(void);
    int RunSubcase1(void);
};

#endif /* PLATFORM_TEST_CASES_BASIC_TIMING_BASIC_TIMING_HH */
