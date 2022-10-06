
#ifndef PLATFORM_TEST_CASES_BASIC_WORKERS_BASIC_WORKERS_HH
#define PLATFORM_TEST_CASES_BASIC_WORKERS_BASIC_WORKERS_HH

#include <framework/test_case.hh>

class TestBasicWorkers : public TestCase::Instance {
public:
    TestBasicWorkers(const char * name) : TestCase::Instance(name) {}
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
    int RunSubcase8(void);
    int RunSubcase9(void);
};

#endif /* PLATFORM_TEST_CASES_BASIC_WORKERS_BASIC_WORKERS_HH */
