
#ifndef PLATFORM_COMPONENTS_INPUT_INPUT_H
#define PLATFORM_COMPONENTS_INPUT_INPUT_H

#include <event_machine.h>

/**
 * @brief Callback passed to em_conf and called periodically to poll external interfaces
 * @return Number of events enqueued into EM
 */
int EmInputPollFunction(void);

/**
 * @brief Hook exposed for the startup component to register via em_hooks_register_send()
 * @param events Table of events being sent
 * @param num Number of events being sent
 * @param queue Target queue
 * @param eventGroup Event group
 */
void EmApiHookSend(const em_event_t events[], int num, em_queue_t queue, em_event_group_t eventGroup);

/**
 * @brief Enable input polling on the current core
 */
void EnableInputPolling(void);

/**
 * @brief Disable further input polling on the current core
 */
void DisableInputPolling(void);

#endif /* PLATFORM_COMPONENTS_INPUT_INPUT_H */
