
#ifndef PLATFORM_TEST_FRAMEWORK_IPC_SOCKET_HH
#define PLATFORM_TEST_FRAMEWORK_IPC_SOCKET_HH

namespace IpcSocket {

void Init(void);
void Teardown(void);
void WriteBack(const char * fmt, ...) __attribute__((format(printf, 1, 2)));

}

#endif /* PLATFORM_TEST_FRAMEWORK_IPC_SOCKET_HH */
