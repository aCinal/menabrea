#include <memory/setup.h>
#include <menabrea/common.h>
#include <menabrea/exception.h>

#define SHARED_MEMORY_MAGIC  ( (u32) 0x008E8041 )

typedef struct SSharedMemoryHeader {
    em_event_t Event;
    env_atomic32_t References;
    u32 Magic;
} SSharedMemoryHeader __attribute__((aligned(8)));  /* Align to 64 bits */

void * GetMemory(u32 size) {

    em_event_t event = em_alloc(size + sizeof(SSharedMemoryHeader), EM_EVENT_TYPE_SW, APP_MEMORY_EVENT_POOL);
    void * ptr = NULL;

    if (likely(event != EM_EVENT_UNDEF)) {

        SSharedMemoryHeader * hdr = (SSharedMemoryHeader *) em_event_pointer(event);
        hdr->Event = event;
        hdr->Magic = SHARED_MEMORY_MAGIC;
        env_atomic32_init(&hdr->References);
        /* Set the reference counter to one */
        env_atomic32_inc(&hdr->References);
        /* Skip the header */
        ptr = hdr + 1;
    }

    /* Return the pointer, note that we rely here on the fact that the event pool
     * was set up before the fork and so the pointer is valid on all cores (in all
     * address spaces) and can be directly used by the application */
    return ptr;
}

void * RefMemory(void * ptr) {

    /* Access the header */
    SSharedMemoryHeader * hdr = (SSharedMemoryHeader *) ptr - 1;
    /* Crash if the application mishandled the pointer */
    AssertTrue(hdr->Magic == SHARED_MEMORY_MAGIC);
    /* Increment the reference counter */
    env_atomic32_inc(&hdr->References);

    return ptr;
}

void PutMemory(void * ptr) {

    /* Access the header */
    SSharedMemoryHeader * hdr = (SSharedMemoryHeader *) ptr - 1;
    /* Crash if the application mishandled the pointer */
    AssertTrue(hdr->Magic == SHARED_MEMORY_MAGIC);
    /* If reference counter is zero, free the memory (note that the platform
     * cannot be held responsible for any race conditions where an attempt at
     * an additional reference is made while deallocation is taking place) */
    if (0 == env_atomic32_sub_return(&hdr->References, 1)) {

        /* Return the event to the pool */
        em_free(hdr->Event);

    }
}
