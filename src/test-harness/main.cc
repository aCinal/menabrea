#include <menabrea/common.h>
#include "ipc_socket.hh"
#include "test_runner.hh"
#include <menabrea/test/test_case.hh>

APPLICATION_GLOBAL_INIT() {

    TestRunner::Init();
    IpcSocket::Init();
}

APPLICATION_LOCAL_INIT(core) {

    /* No per-core init needed */
    (void) core;
}

APPLICATION_LOCAL_EXIT(core) {

    /* No per-core exit needed */
    (void) core;
}

APPLICATION_GLOBAL_EXIT() {

    IpcSocket::Teardown();
    TestRunner::Teardown();
}
