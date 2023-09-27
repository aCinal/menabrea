#include <menabrea/memory.h>
#include <memory/setup.h>
#include <menabrea/common.h>
#include <menabrea/exception.h>
#include <menabrea/log.h>
#include <stdlib.h>

#define RUNTIME_SHM_MAGIC  ( (u32) 0x008E8041 )

typedef struct SRuntimeMemoryHeader {
    u32 Magic;
    env_atomic32_t References;
    em_event_t Event;
} SRuntimeMemoryHeader __attribute__((aligned(8)));  /* Align to 64 bits */

typedef struct SInitMemoryDebt {
    void * Block;
    struct SInitMemoryDebt * Next;
} SInitMemoryDebt;

static SInitMemoryDebt * s_initMemoryDebtStack = NULL;
static bool s_allowInitMemoryAllocations = true;

void * GetInitMemory(u32 size) {

    void * ptr = NULL;
    if (s_allowInitMemoryAllocations) {

        ptr = env_shared_malloc(size);
        if (ptr) {

            /* Push the init memory onto a stack so that it can be relased by the platform at shutdown */
            SInitMemoryDebt * debtEntry = malloc(sizeof(SInitMemoryDebt));
            AssertTrue(debtEntry);
            debtEntry->Block = ptr;
            debtEntry->Next = s_initMemoryDebtStack;
            s_initMemoryDebtStack = debtEntry;
        }

    } else {

        RaiseException(EExceptionFatality_NonFatal, \
            "Init memory can only be allocated at global init time");
    }

    return ptr;
}

void DisableInitMemoryAllocation(void) {

    s_allowInitMemoryAllocations = false;
}

void ReleaseInitMemory(void) {

    LogPrint(ELogSeverityLevel_Info, "Releasing init memory...");

    /* Walk the stack of application allocations and release each block */
    while (s_initMemoryDebtStack) {

        SInitMemoryDebt * debtEntry = s_initMemoryDebtStack;
        SInitMemoryDebt * next = debtEntry->Next;

        /* Release the application memory block */
        env_shared_free(debtEntry->Block);
        /* Release the bookkeeping data */
        free(debtEntry);

        s_initMemoryDebtStack = next;
    }
}

void * GetRuntimeMemory(u32 size) {

    if (size == 0) {

        RaiseException(EExceptionFatality_NonFatal, "Tried allocating zero bytes");
        return NULL;
    }

    void * ptr = NULL;
    em_event_t event = em_alloc(size + sizeof(SRuntimeMemoryHeader), EM_EVENT_TYPE_SW, RUNTIME_SHMEM_EVENT_POOL);
    if (likely(event != EM_EVENT_UNDEF)) {

        SRuntimeMemoryHeader * hdr = (SRuntimeMemoryHeader *) em_event_pointer(event);
        hdr->Event = event;
        hdr->Magic = RUNTIME_SHM_MAGIC;
        env_atomic32_init(&hdr->References);
        /* Set the reference counter to one */
        env_atomic32_inc(&hdr->References);
        /* Skip the header */
        ptr = hdr + 1;
    }

    return ptr;
}

void * RefRuntimeMemory(void * ptr) {

    /* Access the header */
    SRuntimeMemoryHeader * hdr = (SRuntimeMemoryHeader *) ptr - 1;
    /* Crash if the application mishandled the pointer */
    AssertTrue(hdr->Magic == RUNTIME_SHM_MAGIC);
    /* Increment the reference counter */
    env_atomic32_inc(&hdr->References);

    return ptr;
}

void PutRuntimeMemory(void * ptr) {

    /* Access the header */
    SRuntimeMemoryHeader * hdr = (SRuntimeMemoryHeader *) ptr - 1;
    /* Crash if the application mishandled the pointer */
    AssertTrue(hdr->Magic == RUNTIME_SHM_MAGIC);
    /* If reference counter is zero, free the memory (note that the platform
     * cannot be held responsible for any race conditions where an attempt at
     * an additional reference is made while deallocation is taking place) */
    if (0 == env_atomic32_sub_return(&hdr->References, 1)) {

        em_free(hdr->Event);
    }
}
