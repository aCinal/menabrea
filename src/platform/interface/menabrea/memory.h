
#ifndef PLATFORM_INTERFACE_MENABREA_MEMORY_H
#define PLATFORM_INTERFACE_MENABREA_MEMORY_H

#ifdef __cplusplus
extern "C" {
#endif

#include <menabrea/common.h>

/**
 * @brief Request allocation of shared memory of a given size
 * @param size Size of the allocation
 * @return Shared memory address (valid on all cores) or NULL if the request could not be satisfied
 */
void * GetMemory(u32 size);

/**
 * @brief Increment the reference counter of shared memory
 * @param ptr Shared memory pointer previously returned from GetMemory
 * @return ptr
 * @note Memory will not be deallocated until it is put the number of times equal to the number of
         references (GetMemory initially sets the reference counter to one)
 * @see GetMemory, PutMemory
 */
void * RefMemory(void * ptr);

/**
 * @brief Decrement the memory reference counter and free the memory if zero
 * @param ptr Shared memory pointer previously returned from GetMemory
 * @see GetMemory
 */
void PutMemory(void * ptr);

#ifdef __cplusplus
}
#endif

#endif /* PLATFORM_INTERFACE_MENABREA_MEMORY_H */