#include <input/input.h>
#include <menabrea/input.h>
#include <menabrea/common.h>
#include <menabrea/log.h>
#include <menabrea/exception.h>

#define MAX_INPUT_CALLBACKS  16

typedef struct SInputPollCallback {
    TInputPollCallback Callback;
    int CoreMask;
} SInputPollCallback;

static SInputPollCallback s_inputPollCallbacks[MAX_INPUT_CALLBACKS];
static u32 s_numOfCallbacks = 0;
static bool s_pollingEnabled = false;
/* Private (per core) variables used to trace the number of events sent
 * from the context of the input poll function to give a hint to the
 * scheduler */
static bool s_inInputPollCallback = false;
static int s_totalEventsEnqueued = 0;

void RegisterInputPolling(TInputPollCallback callback, int coreMask) {

    if (unlikely(callback == NULL)) {

        RaiseException(EExceptionFatality_NonFatal, "Passed NULL pointer for input callback");
        return;
    }

    if (unlikely(coreMask == 0)) {

        RaiseException(EExceptionFatality_NonFatal, "Empty core mask for callback %p", callback);
        return;
    }

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

    s_inInputPollCallback = true;
    s_totalEventsEnqueued = 0;

    if (likely(s_pollingEnabled)) {

        for (u32 i = 0; i < s_numOfCallbacks; i++) {

            int core = em_core_id();
            if (s_inputPollCallbacks[i].CoreMask & (1 << core)) {

                s_inputPollCallbacks[i].Callback();
            }
        }
    }

    s_inInputPollCallback = false;
    return s_totalEventsEnqueued;
}

void EnableInputPolling(void) {

    s_pollingEnabled = true;
}

void DisableInputPolling(void) {

    s_pollingEnabled = false;
}

void EmApiHookSend(const em_event_t events[], int num, em_queue_t queue, em_event_group_t eventGroup) {

    (void) events;
    (void) queue;
    (void) eventGroup;

    if (unlikely(s_inInputPollCallback)) {

        s_totalEventsEnqueued += num;
    }
}
