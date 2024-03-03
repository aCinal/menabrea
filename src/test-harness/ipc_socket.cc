#define FORMAT_LOG(fmt)  "ipc_socket.cc: " fmt
#include "logging.hh"
#include "ipc_socket.hh"
#include "test_runner.hh"
#include <menabrea/exception.h>
#include <menabrea/input.h>
#include <menabrea/cores.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <poll.h>
#include <stdarg.h>
#include <stdio.h>

static int CreateListeningSocket(const char * name);
static void UdsInputPollCallback(void * arg);
static void HandleConnectionSocketEvent(short events);

static constexpr const char * UDS_NAME = "/tmp/test_framework.sock";
static constexpr const u32 POLLIN_BUF_LEN = 256;
static int s_listeningSocket = -1;
static int s_activeConnection = -1;
static int s_pollableDescriptor = -1;

void IpcSocket::Init(void) {

    /* Request removal of the named socket on disgraceful shutdown. To not risk
     * a race condition we register this cleanup action first, before creating
     * the socket in question. */
    OnDisgracefulShutdown("rm -f %s", UDS_NAME);

    s_listeningSocket = CreateListeningSocket(UDS_NAME);
    /* Set the listening socket as the first descriptor to be polled */
    s_pollableDescriptor = s_listeningSocket;

    /* Request input polling on the shared core only to not worry about passing connection
     * descriptors between processes */
    RegisterInputPolling(UdsInputPollCallback, nullptr, GetSharedCoreMask());
}

void IpcSocket::Teardown(void) {

    /* Terminate any active connections */
    if (s_activeConnection != -1) {
        AssertTrue(0 == close(s_activeConnection));
    }
    /* Close the listening socket */
    AssertTrue(0 == close(s_listeningSocket));

    /* Remove the named socket from the filesystem */
    AssertTrue(0 == unlink(UDS_NAME));
}

void IpcSocket::WriteBack(const char * fmt, ...) {

    /* Assert this function only gets called from the shared core
     * as that's the only core where the connection descriptor is
     * valid */
    AssertTrue(GetCurrentCore() == 0);

    /* Fail silently if connection dropped in the meantime */
    if (s_activeConnection != -1) {

        va_list args;
        va_start(args, fmt);
        vdprintf(s_activeConnection, fmt, args);
        va_end(args);
    }
}

static int CreateListeningSocket(const char * name) {

    /* Create the socket... */
    int fd = socket(AF_UNIX, SOCK_SEQPACKET, 0);
    AssertTrue(fd != -1);
    /* ...assign a name to it... */
    struct sockaddr_un sa = {};
    sa.sun_family = AF_UNIX;
    (void) strncpy(sa.sun_path, name, sizeof(sa.sun_path) - 1);
    sa.sun_path[sizeof(sa.sun_path) - 1] = '\0';
    AssertTrue(0 == bind(fd, (struct sockaddr *) &sa, sizeof(sa)));
    /* ...and mark it as passive with no backlog */
    AssertTrue(0 == listen(fd, 0));
    return fd;
}

static void UdsInputPollCallback(void * arg) {

    (void) arg;

    AssertTrue(s_pollableDescriptor != -1);
    struct pollfd pollfd = {
        .fd = s_pollableDescriptor,
        .events = POLLIN
    };

    int ret = poll(&pollfd, 1, 0);
    if (unlikely(ret == -1)) {

        /* poll is one of the interfaces that do not get restarted after being
         * interrupted by a signal, even if SA_RESTART is used. Handle such
         * interruptions gracefully. */
        AssertTrue(EINTR == errno);
        return;
    }

    if (unlikely(ret > 0)) {

        if (s_pollableDescriptor == s_listeningSocket) {

            /* Activity on the listening socket */

            /* Assert consistency, i.e. no active connection */
            AssertTrue(s_activeConnection == -1);
            /* New connection - assert no error on the listening socket */
            AssertTrue(pollfd.revents == POLLIN);
            s_activeConnection = accept(s_listeningSocket, NULL, NULL);
            if (likely(s_activeConnection != -1)) {

                /* Stop polling the listening socket - run the framework as
                 * a singleton */
                s_pollableDescriptor = s_activeConnection;
                LOG_INFO("New test framework connection accepted at descriptor %d", s_activeConnection);

            } else {

                LOG_ERROR("accept() failed (errno=%d: %s)", \
                    errno, strerror(errno));
            }

        } else {

            /* Activity on the active connection */
            HandleConnectionSocketEvent(pollfd.revents);
        }
    }
}

static void HandleConnectionSocketEvent(short events) {

    AssertTrue(s_activeConnection != -1);

    /* Check for POLLHUP first, since it will come with POLLIN (the socket with
     * the other end disconnected will not block on read, so poll() is technically
     * correct to send us POLLIN) */
    if (events & POLLHUP) {

        /* Peer disconnected */

        /* Close the descriptor */
        AssertTrue(0 == close(s_activeConnection));
        s_activeConnection = -1;
        /* Go back to polling the listening socket */
        s_pollableDescriptor = s_listeningSocket;

        LOG_INFO("Test framework connection closed by remote peer");

        TestRunner::OnPollHup();

    } else if (events & POLLIN) {

        /* Only handle POLLIN when POLLHUP has not been set in events */
        char buffer[POLLIN_BUF_LEN + 1];
        ssize_t bytesRead = read(s_activeConnection, buffer, POLLIN_BUF_LEN);
        /* If POLLIN was returned without POLLHUP, then there must be outstanding data in the channel */
        AssertTrue(bytesRead > 0);
        /* Ensure proper null termination */
        buffer[bytesRead] = '\0';

        /* Pass the buffer to the test runner */
        TestRunner::OnPollIn(buffer);
    }
}
