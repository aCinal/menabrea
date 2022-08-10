#include <input/input.h>
#include <menabrea/input.h>
#include <menabrea/common.h>
#include <menabrea/log.h>
#include <menabrea/exception.h>
#include <event_machine.h>

#define MAX_INPUT_CALLBACKS  16

typedef struct SInputPollCallback {
    TInputPollCallback Callback;
    int CoreMask;
} SInputPollCallback;

static SInputPollCallback s_inputPollCallbacks[MAX_INPUT_CALLBACKS];
static u32 s_numOfCallbacks = 0;
static bool s_pollingEnabled = false;

void RegisterInputPolling(TInputPollCallback callback, int coreMask) {

    if (s_numOfCallbacks < MAX_INPUT_CALLBACKS) {

        s_inputPollCallbacks[s_numOfCallbacks].Callback = callback;
        s_inputPollCallbacks[s_numOfCallbacks].CoreMask = coreMask;
        s_numOfCallbacks++;
        LogPrint(ELogSeverityLevel_Debug, "%s(): Registered input poll callback at %p on cores 0x%x", \
            __FUNCTION__, callback, coreMask);

    } else {

        LogPrint(ELogSeverityLevel_Critical, "%s(): Cannot register callback at %p. Upper limit of %d callbacks reached", \
            __FUNCTION__, callback, s_numOfCallbacks);
    }
}

int EmInputPollFunction(void) {

    int totalEventsEnqueued = 0;

    if (likely(s_pollingEnabled)) {

        for (u32 i = 0; i < s_numOfCallbacks; i++) {

            int core = em_core_id();
            if (s_inputPollCallbacks[i].CoreMask & (1 << core)) {

                int eventsEnqueued = s_inputPollCallbacks[i].Callback();
                /* Raise exception if an application returns a negative value */
                AssertTrue(eventsEnqueued >= 0);
                totalEventsEnqueued += eventsEnqueued;
            }
        }
    }

    return totalEventsEnqueued;
}

void EnableInputPolling(void) {

    s_pollingEnabled = true;
}

void DisableInputPolling(void) {

    s_pollingEnabled = false;
}
