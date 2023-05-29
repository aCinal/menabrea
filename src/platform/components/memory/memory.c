#include <menabrea/memory.h>
#include <memory/setup.h>
#include <menabrea/common.h>
#include <menabrea/exception.h>
#include <sys/mman.h>
#include <unistd.h>
#include <stdlib.h>

#define SHARED_MEMORY_MAGIC  ( (u32) 0x008E8041 )

typedef struct SMemoryHeader {
    u32 Magic;
    env_atomic32_t References;
    long Private;
    EMemoryPool Pool;
} SMemoryHeader __attribute__((aligned(8)));  /* Align to 64 bits */

static inline SMemoryHeader * AllocateMemory(u32 userSize, EMemoryPool pool);
static inline void ReleaseMemory(SMemoryHeader * hdr);
static inline u32 PageAlign(u32 size);

void * GetMemory(u32 size, EMemoryPool pool) {

    if (size == 0) {

        RaiseException(EExceptionFatality_NonFatal, "Tried allocating zero bytes");
        return NULL;
    }

    void * ptr = NULL;
    SMemoryHeader * hdr = AllocateMemory(size, pool);
    if (likely(hdr != NULL)) {

        /* Initialize the common part of the header */
        hdr->Magic = SHARED_MEMORY_MAGIC;
        hdr->Pool = pool;
        env_atomic32_init(&hdr->References);
        /* Set the reference counter to one */
        env_atomic32_inc(&hdr->References);
        /* Skip the header */
        ptr = hdr + 1;
    }

    return ptr;
}

void * RefMemory(void * ptr) {

    /* Access the header */
    SMemoryHeader * hdr = (SMemoryHeader *) ptr - 1;
    /* Crash if the application mishandled the pointer */
    AssertTrue(hdr->Magic == SHARED_MEMORY_MAGIC);
    /* Increment the reference counter */
    env_atomic32_inc(&hdr->References);

    return ptr;
}

void PutMemory(void * ptr) {

    /* Access the header */
    SMemoryHeader * hdr = (SMemoryHeader *) ptr - 1;
    /* Crash if the application mishandled the pointer */
    AssertTrue(hdr->Magic == SHARED_MEMORY_MAGIC);
    /* If reference counter is zero, free the memory (note that the platform
     * cannot be held responsible for any race conditions where an attempt at
     * an additional reference is made while deallocation is taking place) */
    if (0 == env_atomic32_sub_return(&hdr->References, 1)) {

        ReleaseMemory(hdr);
    }
}

static inline SMemoryHeader * AllocateMemory(u32 userSize, EMemoryPool pool) {

    u32 totalSize = userSize + sizeof(SMemoryHeader);
    SMemoryHeader * hdr = NULL;
    em_event_t event;

    switch (pool) {
    case EMemoryPool_Local:
        hdr = malloc(totalSize);
        break;

    case EMemoryPool_SharedInit:
        /* TODO: Add support for hugepages here, e.g. EMemoryPool_SharedInitHuge */
        hdr = mmap(
            NULL,
            PageAlign(totalSize),
            PROT_READ | PROT_WRITE,
            MAP_ANONYMOUS | MAP_SHARED,
            -1,
            0
        );
        /* mmap returns MAP_FAILED, not NULL, on failure */
        if (likely(hdr != MAP_FAILED)) {

            /* Save the number of pages as we will need it
             * should we want to call munmap */
            hdr->Private = PageAlign(totalSize);

        } else {

            hdr = NULL;
        }
        break;

    case EMemoryPool_SharedRuntime:
        event = em_alloc(totalSize, EM_EVENT_TYPE_SW, RUNTIME_SHMEM_EVENT_POOL);
        if (likely(event != EM_EVENT_UNDEF)) {

            hdr = (SMemoryHeader *) em_event_pointer(event);
            hdr->Private = (long) event;
        }
        break;

    default:
        RaiseException(EExceptionFatality_NonFatal, "Invalid memory pool %d", pool);
        break;
    }

    return hdr;
}

static inline void ReleaseMemory(SMemoryHeader * hdr) {

    switch (hdr->Pool) {
    case EMemoryPool_Local:
        free(hdr);
        break;

    case EMemoryPool_SharedInit:
        /* Init memory is never released, reference counting in this case
         * makes no sense as there are separate mappings in different
         * processes which would need to be destroyed independently */
        RaiseException(EExceptionFatality_NonFatal, \
            "Attempted to release init memory at %p", hdr + 1);
        /* Unmap in the current process anyway to not have a leak if we
         * ever want to test this behaviour */
        AssertTrue(0 == munmap(hdr, hdr->Private));
        break;

    case EMemoryPool_SharedRuntime:
        em_free((em_event_t) hdr->Private);
        break;

    default:
        RaiseException(EExceptionFatality_Fatal, \
            "Invalid memory pool %d on memory header at %p", \
            hdr->Pool, hdr);
        break;
    }
}

static inline u32 PageAlign(u32 size) {

    u32 page_size = (u32) sysconf(_SC_PAGESIZE);
    /* Page size is always a power of two, instead of
     * using the remainder operator, use a bitmask */
    return (size + page_size) & ~(page_size - 1);
}
