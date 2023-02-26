
#ifndef PLATFORM_TEST_FRAMEWORK_TEST_CASE_HH
#define PLATFORM_TEST_FRAMEWORK_TEST_CASE_HH

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

}

#endif /* PLATFORM_TEST_FRAMEWORK_TEST_CASE_HH */
