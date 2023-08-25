
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
 * @note Memory obtained via this function is shared by all cores only if allocated during global
 *       init. Otherwise the memory is only valid on the calling core.
 */
static inline void * GetInitMemory(u32 size) { return env_shared_malloc(size); }

/**
 * @brief Reserve named shared memory region and return an address to it
 * @param name Name of the shared memory region
 * @param size Size of the allocation
 * @return Memory address or NULL if the request could not be satisfied
 * @note If this is called during global init, the address remains valid on all cores after the fork. Otherwise,
 *       remaining cores must call AttachNamedMemory to get access to the shared memory region
 * @see AttachNamedMemory
 */
static inline void * ReserveNamedMemory(const char * name, u32 size) { return env_shared_reserve(name, size); }

/**
 * @brief Look up a shared memory region by name
 * @param name Name of the shared memory region
 * @return Memory address or NULL if lookup failed
 * @warning Pointers to the named memory need not be the same on all cores and thus should not be shared
 * @see ReserveNamedMemory
 */
static inline void * AttachNamedMemory(const char * name) { return env_shared_lookup(name); }

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
