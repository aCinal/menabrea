
#ifndef PLATFORM_INTERFACE_MENABREA_MEMORY_H
#define PLATFORM_INTERFACE_MENABREA_MEMORY_H

#ifdef __cplusplus
extern "C" {
#endif

#include <menabrea/common.h>

/**
 * @brief Enumeration of memory types (pools)
 */
typedef enum EMemoryPool {
    EMemoryPool_Local = 0,     /**< Pool for local allocations, pointers returned from this pool shall only be valid on the current core */
    EMemoryPool_SharedInit,    /**< Pool suitable for large shared memory allocations during application startup, memory obtained this way cannot be released */
    EMemoryPool_SharedRuntime  /**< Pool of memory from preallocated arenas, shared between cores - pointers returned from this pool shall be valid on all cores */
} EMemoryPool;

/**
 * @brief Request allocation of memory buffer of a given size
 * @param size Size of the allocation
 * @param pool Memory pool
 * @return Memory address or NULL if the request could not be satisfied
 * @see EMemoryPool
 */
void * GetMemory(u32 size, EMemoryPool pool);

/**
 * @brief Increment the reference counter of a memory buffer
 * @param ptr Pointer to a memory buffer previously returned from GetMemory
 * @return ptr
 * @note Memory will not be deallocated until it is put the number of times equal to the number of
         references (GetMemory initially sets the reference counter to one)
 * @see GetMemory, PutMemory
 */
void * RefMemory(void * ptr);

/**
 * @brief Decrement the memory reference counter and free the memory if zero
 * @param ptr Pointer to a memory buffer previously returned from GetMemory
 * @warning It is an error to attempt to put memory from pool EMemoryPool_SharedInit
 * @see GetMemory, EMemoryPool
 */
void PutMemory(void * ptr);

#ifdef __cplusplus
}
#endif

#endif /* PLATFORM_INTERFACE_MENABREA_MEMORY_H */
