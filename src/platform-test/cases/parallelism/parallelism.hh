
#ifndef PLATFORM_TEST_CASES_PARALLELISM_PARALLELISM_HH
#define PLATFORM_TEST_CASES_PARALLELISM_PARALLELISM_HH

#include <menabrea/test/test_case.hh>

class TestParallelism : public TestCase::Instance {
public:
    TestParallelism(const char * name) : TestCase::Instance(name) {}
    virtual u32 GetParamsSize(void) override;
    virtual int ParseParams(char * paramsIn, void * paramsOut) override;
    virtual int StartTest(void * args) override;
    virtual void StopTest(void) override;

private:
    int RunAtomics(void);
    int RunSpinlocks(void);
};

#endif /* PLATFORM_TEST_CASES_PARALLELISM_PARALLELISM_HH */
