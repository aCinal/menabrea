
#ifndef PLATFORM_INTERFACE_MENABREA_CORES_H
#define PLATFORM_INTERFACE_MENABREA_CORES_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Return the zero-based index of the current core
 * @return Index of the current core
 */
int GetCurrentCore(void);

/**
 * @brief Return the core mask corresponding to the current core
 * @return Mask of the current core
 */
int GetCurrentCoreMask(void);

/**
 * @brief Return a core mask corresponding to the shared core (suitable for non-critical applications)
 * @return Mask of the shared core
 * @note Platform operates on the assumption that one physical core is shared between EM-ODP
 *       and other Linux processes, while the rest are isolated (e.g. via kernel command-line)
 */
int GetSharedCoreMask(void);

/**
 * @brief Return a core mask corresponding to isolated cores only (suitable for high-performance low-latency applications)
 * @return Mask of isolated cores
 * @note Platform operates on the assumption that one physical core is shared between EM-ODP
 *       and other Linux processes, while the rest are isolated (e.g. via kernel command-line)
 */
int GetIsolatedCoresMask(void);

/**
 * @brief Return a core mask corresponding to all cores of the system
 * @return Mask of all cores
 */
static inline int GetAllCoresMask(void) {

    return GetSharedCoreMask() | GetIsolatedCoresMask();
}

#ifdef __cplusplus
}
#endif

#endif /* PLATFORM_INTERFACE_MENABREA_CORES_H */
