#include <menabrea/log.h>
#include <menabrea/test/test_case.hh>
#include <menabrea/test/params_parser.hh>

class TestDummy : public TestCase::Instance {
    struct TestParams {
        u32 DummyParam;
    };

public:
    TestDummy(const char * name) : TestCase::Instance(name) {}
    virtual u32 GetParamsSize(void) override {
        return sizeof(TestParams);
    }

    virtual int ParseParams(char * paramsIn, void * paramsOut) override {

        ParamsParser::StructLayout paramsLayout;
        paramsLayout["dummy"] = ParamsParser::StructField(offsetof(TestParams, DummyParam), sizeof(u32), ParamsParser::FieldType::U32);

        return ParamsParser::Parse(paramsIn, paramsOut, std::move(paramsLayout));
    }

    virtual int StartTest(void * args) override {

        TestParams * params = static_cast<TestParams *>(args);
        TestCase::ReportTestResult(TestCase::Result::Success, "Parsed DummyParam=%d", params->DummyParam);
        return 0;
    }

    virtual void StopTest(void) override {

    }
};

APPLICATION_GLOBAL_INIT() {

    TestCase::Register(new TestDummy("TestDummy"));
}

APPLICATION_LOCAL_INIT(core) {

    (void) core;
}

APPLICATION_LOCAL_EXIT(core) {

    (void) core;
}

APPLICATION_GLOBAL_EXIT() {

    delete TestCase::Deregister("TestDummy");
}
