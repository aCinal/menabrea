#include <framework/ipc_socket.hh>
#include <framework/test_runner.hh>
#include <framework/test_case.hh>

#include <cases/basic_timing/basic_timing.hh>
#include <cases/basic_workers/basic_workers.hh>
#include <cases/message_buffering/message_buffering.hh>
#include <cases/messaging_performance/messaging_performance.hh>
#include <cases/oneshot_timer/oneshot_timer.hh>
#include <cases/parallelism/parallelism.hh>
#include <cases/periodic_timer/periodic_timer.hh>
#include <cases/shared_memory/shared_memory.hh>

static void RegisterTestCases(void);

extern "C" void ApplicationGlobalInit(void) {

    TestRunner::Init();
    IpcSocket::Init();

    TestCase::Register(new TestBasicTiming("TestBasicTiming"));
    TestCase::Register(new TestBasicWorkers("TestBasicWorkers"));
    TestCase::Register(new TestMessageBuffering("TestMessageBuffering"));
    TestCase::Register(new TestMessagingPerformance("TestMessagingPerformance"));
    TestCase::Register(new TestOneshotTimer("TestOneshotTimer"));
    TestCase::Register(new TestParallelism("TestParallelism"));
    TestCase::Register(new TestPeriodicTimer("TestPeriodicTimer"));
    TestCase::Register(new TestSharedMemory("TestSharedMemory"));
}

extern "C" void ApplicationLocalInit(int core) {

    /* No per-core init needed */
    (void) core;
}

extern "C" void ApplicationLocalExit(int core) {

    /* No per-core exit needed */
    (void) core;
}

extern "C" void ApplicationGlobalExit(void) {

    IpcSocket::Teardown();
    TestRunner::Teardown();

    delete TestCase::Deregister("TestBasicTiming");
    delete TestCase::Deregister("TestBasicWorkers");
    delete TestCase::Deregister("TestMessagingPerformance");
    delete TestCase::Deregister("TestMessageBuffering");
    delete TestCase::Deregister("TestOneshotTimer");
    delete TestCase::Deregister("TestParallelism");
    delete TestCase::Deregister("TestPeriodicTimer");
    delete TestCase::Deregister("TestSharedMemory");
}
