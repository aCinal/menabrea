
#ifndef PLATFORM_TEST_CASES_BASIC_TIMING_BASIC_TIMING_HH
#define PLATFORM_TEST_CASES_BASIC_TIMING_BASIC_TIMING_HH

#include <menabrea/test/test_case.hh>

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
    int RunSubcase2(void);
    int RunSubcase3(void);
    int RunSubcase4(void);
    int RunSubcase5(void);
    int RunSubcase6(void);
    int RunSubcase7(void);
};

#endif /* PLATFORM_TEST_CASES_BASIC_TIMING_BASIC_TIMING_HH */
