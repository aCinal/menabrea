#define FORMAT_LOG(fmt)  "test_case.cc: " fmt
#include <framework/logging.hh>
#include <framework/test_case.hh>
#include <map>

namespace TestCase {

static std::map<std::string, Instance *> s_casesDatabase;

Instance * GetInstanceByName(const char * name) {

    return s_casesDatabase.find(name) != s_casesDatabase.end() ? s_casesDatabase.at(name) : nullptr;
}

int Register(const char * name, Instance * instance) {

    if (GetInstanceByName(name) == nullptr) {

        s_casesDatabase[name] = instance;
        return 0;

    } else {

        LogPrint(ELogSeverityLevel_Warning, "Cannot register test case '%s'. Name already in use", name);
        return -1;
    }
}

Instance * Deregister(const char * name) {

    Instance * instance = GetInstanceByName(name);
    if (instance != nullptr) {

        s_casesDatabase.erase(name);

    } else {

        LogPrint(ELogSeverityLevel_Warning, "Cannot deregister test case '%s'. Test case not registered", name);
    }

    return instance;
}

}
