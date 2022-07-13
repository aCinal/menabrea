
#ifndef PLATFORM_COMPONENTS_WORK_CORE_QUEUE_GROUPS_H
#define PLATFORM_COMPONENTS_WORK_CORE_QUEUE_GROUPS_H

#include <event_machine.h>

/**
 * @brief Set up queue groups for each core combination in shared memory for quick lookup
 */
void SetUpQueueGroups(void);

/**
 * @brief Map a core mask
 */
em_queue_group_t MapCoreMaskToQueueGroup(int coreMask);

/**
 * @brief Tear down queue groups previously set with a call to SetUpQueueGroups
 * @see SetUpQueueGroups
 */
void TearDownQueueGroups(void);

#endif /* PLATFORM_COMPONENTS_WORK_CORE_QUEUE_GROUPS_H */
