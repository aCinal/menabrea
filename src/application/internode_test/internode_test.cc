#include <menabrea/workers.h>
#include <menabrea/log.h>
#include <menabrea/messaging.h>
#include <menabrea/exception.h>
#include <menabrea/timing.h>
#include <menabrea/cores.h>

static void WorkerBody(TMessage message);
static void HandleTimeout(void);

#define STATIC_WORKER_ID   0x0123
#define INTERNODE_MESSAGE  0xABBA
#define TIMEOUT_MESSAGE    0xB0B0
#define GREETING_SIZE      512

struct SInternodeMessage {
    char Greeting[GREETING_SIZE];
};

struct STimeoutMessage {
    TTimerId Timer;
};

APPLICATION_GLOBAL_INIT() {

    TWorkerId workerId = DeploySimpleParallelWorker("internoder", STATIC_WORKER_ID, GetSharedCoreMask(), WorkerBody);
    if (unlikely(WORKER_ID_INVALID == workerId)) {

        LogPrint(ELogSeverityLevel_Error, "Failed to initialize the worker");
        return;
    }

    TTimerId timer = CreateTimer("poll_timer");
    if (unlikely(TIMER_ID_INVALID == timer)) {

        LogPrint(ELogSeverityLevel_Error, "Failed to create the timer");
        TerminateWorker(workerId);
        return;
    }

    TMessage timeoutMessage = CreateMessage(TIMEOUT_MESSAGE, sizeof(STimeoutMessage));
    if (unlikely(timeoutMessage == MESSAGE_INVALID)) {

        LogPrint(ELogSeverityLevel_Error, "Failed to create the timeout message");
        TerminateWorker(workerId);
        DestroyTimer(timer);
        return;
    }

    STimeoutMessage * payload = static_cast<STimeoutMessage *>(GetMessagePayload(timeoutMessage));
    payload->Timer = timer;
    if (unlikely(timer != ArmTimer(timer, 3 * 1000 * 1000, 3 * 1000 * 1000, timeoutMessage, workerId))) {

        LogPrint(ELogSeverityLevel_Error, "Failed to arm the timer");
        TerminateWorker(workerId);
        DestroyTimer(timer);
        return;
    }

    LogPrint(ELogSeverityLevel_Info, "Successfully initialized the internode test!");
}

APPLICATION_LOCAL_INIT(core) {

    (void) core;
}

APPLICATION_LOCAL_EXIT(core) {

    (void) core;
}

APPLICATION_GLOBAL_EXIT() {

    TerminateWorker(MakeWorkerId(GetOwnNodeId(), STATIC_WORKER_ID));
}

static void WorkerBody(TMessage message) {

    switch (GetMessageId(message)) {
    case INTERNODE_MESSAGE:

        LogPrint(ELogSeverityLevel_Info, "Worker 0x%x received internode message from 0x%x (payload size: %d)", \
            GetOwnWorkerId(), GetMessageSender(message), GetMessagePayloadSize(message));
        DestroyMessage(message);
        break;

    case TIMEOUT_MESSAGE:

        DestroyMessage(message);
        HandleTimeout();
        break;

    default:
        LogPrint(ELogSeverityLevel_Warning, "Worker 0x%x received unexpected message 0x%x from 0x%x", \
            GetOwnWorkerId(), GetMessageId(message), GetMessageSender(message));
        DestroyMessage(message);
        break;
    }
}

static void HandleTimeout(void) {

    /* Worker deployed on single core - use static variable */
    static int tickTock = 0;

    for (TWorkerId node = 1; node <= MAX_NODE_ID; node++) {

        if (WorkerIdGetNode(GetOwnWorkerId()) != node) {

            TWorkerId receiver = MakeWorkerId(node, STATIC_WORKER_ID);
            TMessage message = CreateMessage(INTERNODE_MESSAGE, sizeof(SInternodeMessage) * tickTock);
            if (unlikely(message == MESSAGE_INVALID)) {

                LogPrint(ELogSeverityLevel_Error, "Failed to create internode message destined to 0x%x", \
                    receiver);
                return;
            }

            if (tickTock) {
                /* Only every other timeout results in messages with payload */
                SInternodeMessage * payload = static_cast<SInternodeMessage *>(GetMessagePayload(message));
                (void) snprintf(payload->Greeting, sizeof(payload->Greeting), \
                    "Hello 0x%x, this is 0x%x!", receiver, GetOwnWorkerId());
            }

            SendMessage(message, receiver);
        }
    }

    /* Tick the tock */
    tickTock = !tickTock;
}
