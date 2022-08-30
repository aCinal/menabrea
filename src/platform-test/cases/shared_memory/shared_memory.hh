
#ifndef PLATFORM_TEST_CASES_SHARED_MEMORY_SHARED_MEMORY_HH
#define PLATFORM_TEST_CASES_SHARED_MEMORY_SHARED_MEMORY_HH

#include <framework/test_case.hh>

class TestSharedMemory : public TestCase::Instance {
public:
    TestSharedMemory(const char * name) : TestCase::Instance(name) {}
    virtual u32 GetParamsSize(void) override;
    virtual int ParseParams(char * paramsIn, void * paramsOut) override;
    virtual int StartTest(void * args) override;
    virtual void StopTest(void) override;
};

#endif /* PLATFORM_TEST_CASES_SHARED_MEMORY_SHARED_MEMORY_HH */
