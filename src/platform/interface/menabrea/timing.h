
#ifndef PLATFORM_INTERFACE_MENABREA_TIMING_H
#define PLATFORM_INTERFACE_MENABREA_TIMING_H

#ifdef __cplusplus
extern "C" {
#endif

#include <menabrea/common.h>
#include <menabrea/messaging.h>
#include <menabrea/workers.h>

typedef u16 TTimerId;                            /**< Timer identifier */
#define TIMER_ID_INVALID  ( (TTimerId) 0xFFFF )  /**< Magic value used to indicate timer creation failure */
#define MAX_TIMER_COUNT   512                    /**< Maximum number of timers supported by the platform */

/* Assert consistency between constants at compile time */
ODP_STATIC_ASSERT(TIMER_ID_INVALID > MAX_TIMER_COUNT, \
    "TIMER_ID_INVALID must be outside the MAX_TIMER_COUNT range");

/**
 * @brief Create a timer
 * @param name Human-readable timer name
 * @return Timer ID or TIMER_ID_INVALID on failure
 * @see TIMER_ID_INVALID
 */
TTimerId CreateTimer(const char * name);

/**
 * @brief Arm a timer
 * @param timerId Timer ID
 * @param expires Timer expiration time in microseconds
 * @param period Timer period in microseconds (zero if one-shot timer)
 * @param message Message to be sent on timeout
 * @param receiver Receiver of the message
 * @return Timer ID or TIMER_ID_INVALID on failure
 * @note If a call to this function succeeds (return value is not TIMER_ID_INVALID), the ownership
 *       of the message is relinquished and the platform is responsible for the message delivery
 *       or destruction
 * @see TIMER_ID_INVALID
 */
TTimerId ArmTimer(TTimerId timerId, u64 expires, u64 period, TMessage message, TWorkerId receiver);

/**
 * @brief Cancel any pending timeouts and disarm a timer
 * @param timerId Timer ID
 * @return Timer ID or TIMER_ID_INVALID on failure
 */
TTimerId DisarmTimer(TTimerId timerId);

/**
 * @brief Destroy a timer
 * @param timerId Timer ID
 */
void DestroyTimer(TTimerId timerId);

#ifdef __cplusplus
}
#endif

#endif /* PLATFORM_INTERFACE_MENABREA_TIMING_H */
