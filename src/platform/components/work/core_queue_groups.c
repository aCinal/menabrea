
#include "core_queue_groups.h"
#include <menabrea/log.h>
#include <menabrea/exception.h>
#include <stdio.h>
#include <stdlib.h>

typedef struct SCoreGroups {
    int Count;
    em_queue_group_t Groups[0];
} SCoreGroups;

static SCoreGroups * s_coreGroups = NULL;

void SetUpQueueGroups(void) {

    int cores = em_core_count();
    /* Get the number of subsets of cores. Exclude the empty set as there
     * is no point in creating a queue group for no cores. */
    int groupCount = (1 << cores) - 1;
    LogPrint(ELogSeverityLevel_Info, "%s(): Setting up %d queue groups...", \
        __FUNCTION__, groupCount);
    s_coreGroups = malloc(sizeof(SCoreGroups) + groupCount * sizeof(em_queue_group_t));
    AssertTrue(s_coreGroups != NULL);

    s_coreGroups->Count = groupCount;

    for (int i = 0; i < groupCount; i++) {

        char maskString[16];
        /* The index encodes the core mask */
        (void) snprintf(maskString, sizeof(maskString), "0x%x", i + 1);

        em_core_mask_t coreMask;
        em_core_mask_zero(&coreMask);
        em_core_mask_set_str(maskString, &coreMask);

        /* Use a hashmap - core mask indexes the array (up to an offset of one) */
        s_coreGroups->Groups[i] = em_queue_group_create(NULL, &coreMask, 0, NULL);
        AssertTrue(s_coreGroups->Groups[i] != EM_QUEUE_GROUP_UNDEF);
    }
}

em_queue_group_t MapCoreMaskToQueueGroup(int coreMask) {

    /* Subtract one from the coreMask to get an index into the groups array */
    int index = coreMask - 1;
    AssertTrue(index >= 0 && index < s_coreGroups->Count);
    return s_coreGroups->Groups[index];
}

void TearDownQueueGroups(void) {

    /* Use idempotent implementation and allow calling this function
     * early on before initialization completed */
    if (s_coreGroups) {

        for (int i = 0; i < s_coreGroups->Count; i++) {

            AssertTrue(EM_OK == em_queue_group_delete(s_coreGroups->Groups[i], 0, NULL));
        }

        free(s_coreGroups);
        s_coreGroups = NULL;
    }
}
