#define FORMAT_LOG(fmt)  "test_case.cc: " fmt
#include <logging.hh>
#include <test_runner.hh>
#include <menabrea/test/test_case.hh>
#include <map>

namespace TestCase {

static std::map<std::string, Instance *> s_casesDatabase;

Instance * GetInstanceByName(const char * name) {

    return s_casesDatabase.find(name) != s_casesDatabase.end() ? s_casesDatabase.at(name) : nullptr;
}

int Register(Instance * instance) {

    if (GetInstanceByName(instance->GetName()) == nullptr) {

        s_casesDatabase[instance->GetName()] = instance;
        return 0;

    } else {

        LOG_WARNING("Cannot register test case '%s'. Name already in use", \
            instance->GetName());
        return -1;
    }
}

Instance * Deregister(const char * name) {

    Instance * instance = GetInstanceByName(name);
    if (instance != nullptr) {

        s_casesDatabase.erase(name);

    } else {

        LOG_WARNING("Cannot deregister test case '%s'. Test case not registered", name);
    }

    return instance;
}

void ReportTestResult(Result result, const char * extra, ...) {

    va_list args;
    va_start(args, extra);
    TestRunner::ReportTestResult(result, extra, args);
    va_end(args);
}

void ExtendTimeout(u64 remainingTime) {

    TestRunner::ExtendTimeout(remainingTime);
}

}
