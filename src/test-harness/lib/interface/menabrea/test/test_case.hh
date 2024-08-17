
#ifndef TEST_HARNESS_INTERFACE_MENABREA_TEST_TEST_CASE_HH
#define TEST_HARNESS_INTERFACE_MENABREA_TEST_TEST_CASE_HH

#include <menabrea/common.h>
#include <string>

namespace TestCase {

enum class Result {
    Success,
    Failure
};

class Instance {
public:
    Instance(std::string name) : Name(name) {}
    virtual ~Instance(void) {}

    virtual u32 GetParamsSize(void) = 0;
    virtual int ParseParams(char * paramsIn, void * paramsOut) = 0;
    virtual int StartTest(void * params) = 0;
    virtual void StopTest(void) = 0;

    const char * GetName(void) { return this->Name.c_str(); }

private:
    std::string Name;
};

Instance * GetInstanceByName(const char * name);
int Register(Instance * instance);
Instance * Deregister(const char * name);

constexpr const u64 DEFAULT_TEST_TIMEOUT = 5 * 1000 * 1000;  /* 5 seconds */

void ReportTestResult(Result result, const char * extra = " ", ...) __attribute__((format(printf, 2, 3)));
void ExtendTimeout(u64 remainingTime = DEFAULT_TEST_TIMEOUT);

}

#endif /* TEST_HARNESS_INTERFACE_MENABREA_TEST_TEST_CASE_HH */
