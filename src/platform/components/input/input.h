
#ifndef PLATFORM_COMPONENTS_INPUT_INPUT_H
#define PLATFORM_COMPONENTS_INPUT_INPUT_H

/**
 * @brief Callback passed to em_conf and called periodically to poll external interfaces
 * @return Number of events enqueued into EM
 */
int EmInputPollFunction(void);

/**
 * @brief Enable input polling on the current core
 */
void EnableInputPolling(void);

/**
 * @brief Disable further input polling on the current core
 */
void DisableInputPolling(void);

#endif /* PLATFORM_COMPONENTS_INPUT_INPUT_H */
