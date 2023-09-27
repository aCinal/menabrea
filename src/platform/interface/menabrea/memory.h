
#ifndef PLATFORM_INTERFACE_MENABREA_MEMORY_H
#define PLATFORM_INTERFACE_MENABREA_MEMORY_H

#ifdef __cplusplus
extern "C" {
#endif

#include <menabrea/common.h>

/**
 * @brief Allocate shared memory at global init time
 * @param size Size of the allocation
 * @return Memory address or NULL if the request could not be satisfied
 * @note This function can only be used at global init time. Memory obtained this way
 *       is automatically released by the platform during shutdown.
 */
void * GetInitMemory(u32 size);

/**
 * @brief Allocate shared, reference-counted memory at runtime
 * @param size Size of the allocation
 * @return Memory address or NULL if the request could not be satisfied
 */
void * GetRuntimeMemory(u32 size);

/**
 * @brief Increment the reference counter of a memory buffer
 * @param ptr Pointer to a memory buffer previously returned from GetRuntimeMemory
 * @return ptr
 * @note Memory will not be deallocated until it is put the number of times equal to the number of
         references (GetRuntimeMemory initially sets the reference counter to one)
 * @see GetRuntimeMemory, PutRuntimeMemory
 */
void * RefRuntimeMemory(void * ptr);

/**
 * @brief Decrement the memory reference counter and free the memory if zero
 * @param ptr Pointer to a memory buffer previously returned from GetRuntimeMemory
 * @see GetRuntimeMemory
 */
void PutRuntimeMemory(void * ptr);

#ifdef __cplusplus
}
#endif

#endif /* PLATFORM_INTERFACE_MENABREA_MEMORY_H */
