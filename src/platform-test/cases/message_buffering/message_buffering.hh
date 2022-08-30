
#ifndef PLATFORM_TEST_CASES_MESSAGE_BUFFERING_MESSAGE_BUFFERING_HH
#define PLATFORM_TEST_CASES_MESSAGE_BUFFERING_MESSAGE_BUFFERING_HH

#include <framework/test_case.hh>

class TestMessageBuffering : public TestCase::Instance {
public:
    TestMessageBuffering(const char * name) : TestCase::Instance(name) {}
    virtual u32 GetParamsSize(void) override;
    virtual int ParseParams(char * paramsIn, void * paramsOut) override;
    virtual int StartTest(void * args) override;
    virtual void StopTest(void) override;
};

#endif /* PLATFORM_TEST_CASES_MESSAGE_BUFFERING_MESSAGE_BUFFERING_HH */
