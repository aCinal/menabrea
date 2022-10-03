
#ifndef PLATFORM_INTERFACE_MENABREA_WORKERS_H
#define PLATFORM_INTERFACE_MENABREA_WORKERS_H

#ifdef __cplusplus
extern "C" {
#endif

#include <menabrea/common.h>
#include <event_machine.h>

typedef u16 TWorkerId;                                                         /**< Worker identifier type */
typedef em_event_t TMessage;                                                   /**< Opaque message handle */
#define MESSAGE_INVALID           EM_EVENT_UNDEF                               /**< Magic value to signal message allocation failure */

#define WORKER_ID_INVALID         ( (TWorkerId) 0xFFFF )                       /**< Magic value used to indicate worker deployment failure and request to allocate ID dynamically */
#define WORKER_ID_DYNAMIC_BASE    ( (TWorkerId) 0x07FF )                       /**< Boundary value between static worker ID pool and dynamic allocation pool */

#define WORKER_LOCAL_ID_MASK      0x0FFF                                       /**< Mask of the local part of the worker ID */
#define WORKER_LOCAL_ID_BITS      12                                           /**< Bitlength of the local part of the worker ID */
#define WORKER_NODE_ID_MASK       0xF000                                       /**< Mask of the node part of the worker ID */
#define WORKER_NODE_ID_BITS       4                                            /**< Bitlength of the node part of the worker ID */
#define WORKER_NODE_ID_SHIFT      WORKER_LOCAL_ID_BITS                         /**< Shift of the node ID in a global (fully qualified) worker ID */

#define MAX_NODE_ID               3                                            /**< Maximum value of the node ID (equal to the number of nodes in the system) */
#define MAX_WORKER_COUNT          (1 << WORKER_LOCAL_ID_BITS)                  /**< Maximum number of workers deployable */
#define DYNAMIC_WORKER_IDS_COUNT  (MAX_WORKER_COUNT - WORKER_ID_DYNAMIC_BASE)  /**< Number of available dynamic worker IDs */

/* Assert consistency between constants at compile time */
ODP_STATIC_ASSERT(WORKER_ID_DYNAMIC_BASE < MAX_WORKER_COUNT, \
    "WORKER_ID_DYNAMIC_BASE must be less than MAX_WORKER_COUNT");
ODP_STATIC_ASSERT(WORKER_LOCAL_ID_BITS < sizeof(TWorkerId) * 8, \
    "WORKER_LOCAL_ID_BITS too large");
ODP_STATIC_ASSERT(WORKER_LOCAL_ID_BITS + WORKER_NODE_ID_BITS == sizeof(TWorkerId) * 8, \
    "WORKER_LOCAL_ID_BITS + WORKER_NODE_ID_BITS inconsistent");
ODP_STATIC_ASSERT(WORKER_ID_INVALID > MAX_WORKER_COUNT, \
    "WORKER_ID_INVALID must be outside the MAX_WORKER_COUNT range");

/**
 * @brief Retrieve the node part of a worker ID
 * @param id Worker ID
 * @return Node part of a worker ID
 */
static inline TWorkerId WorkerIdGetNode(TWorkerId id) {

    return (id & WORKER_NODE_ID_MASK) >> WORKER_NODE_ID_SHIFT;
}

/**
 * @brief Retrieve the local part of a worker ID
 * @param id Worker ID
 * @return Local part of a worker ID
 */
static inline TWorkerId WorkerIdGetLocal(TWorkerId id) {

    return (id & WORKER_LOCAL_ID_MASK);
}

/**
 * @brief Combine node ID and local ID of the worker
 * @param nodeId Node ID of the worker
 * @param localId Local worker ID
 * @return Fully qualified global worker ID
 */
static inline TWorkerId MakeWorkerId(TWorkerId nodeId, TWorkerId localId) {

    return ( (nodeId << WORKER_NODE_ID_SHIFT) & WORKER_NODE_ID_MASK ) \
        | (localId & WORKER_LOCAL_ID_MASK);
}

/**
 * @brief Get current node's ID
 * @return Current node's ID
 */
TWorkerId GetOwnNodeId(void);

/**
 * @brief User-provided global initialization function
 * @note Return 0 to indicate success and that the framework should proceed with deployment,
 *       non-zero return value indicates failure and deployment will cease
 */
typedef int (* TUserInitCallback)(void * arg);

/**
 * @brief User-provided per-core init function
 * @note This function will be called only on cores on which the worker was deployed based on
 *       the core mask provided in its config. This function presents an opportunity for the
 *       application to look up on all cores the shared memory allocated in the global init.
 */
typedef void (* TUserLocalInitCallback)(int core);

/**
 * @brief User-provided per-core teardown function
 * @note This function will be called only on cores on which the worker was deployed based on
 *       the core mask provided in its config.
 */
typedef void (* TUserLocalExitCallback)(int core);

/**
 * @brief User-provided global teardown function
 */
typedef void (* TUserExitCallback)(void);

/**
 * @brief Worker body defining main functionality of the worker
 * @note This function will get called when a message is sent to the worker
 */
typedef void (* TUserHandlerCallback)(TMessage message);

/**
 * @brief Configuration used during worker deployment
 * @see DeployWorker
 */
typedef struct SWorkerConfig {
    const char * Name;                     /**< Human-readable name */
    void * InitArg;                        /**< Private argument passed to the UserInit function if any provided */
    TWorkerId WorkerId;                    /**< Worker identifier */
    int CoreMask;                          /**< Mask of cores on which the worker is eligible to run */
    bool Parallel;                         /**< Flag denoting whether the worker can be run in parallel on multiple cores at the same time */
    TUserInitCallback UserInit;            /**< User-provided global initialization function */
    TUserLocalInitCallback UserLocalInit;  /**< User-provided per-core initialization function */
    TUserLocalExitCallback UserLocalExit;  /**< User-provided per-core teardown function */
    TUserExitCallback UserExit;            /**< User-provided global teardown function */
    TUserHandlerCallback WorkerBody;       /**< Worker body */
} SWorkerConfig;

/**
 * @brief Create and start a worker
 * @param config Worker configuration
 * @return Worker ID on success, WORKER_ID_INVALID on failure
 * @see WORKER_ID_INVALID
 */
TWorkerId DeployWorker(const SWorkerConfig * config);

/**
 * @brief Deploy a simple worker that can be only be run atomically on a single core at a time
 * @param name Human-readable name (optional)
 * @param id Static worker ID or WORKER_ID_INVALID to get a dynamic worker ID
 * @param coreMask Mask of cores on which the worker is allowed to run
 * @param body Worker body executed when a message is sent to the worker
 * @return Worker ID on success, WORKER_ID_INVALID on failure
 * @see WORKER_ID_INVALID
 */
static inline TWorkerId DeploySimpleWorker(const char * name, TWorkerId id, int coreMask, TUserHandlerCallback body) {

    SWorkerConfig config = {
        .Name = name,
        .WorkerId = id,
        .CoreMask = coreMask,
        .Parallel = false,
        .WorkerBody = body,
    };
    return DeployWorker(&config);
}

/**
 * @brief Deploy a simple worker that can be run in parallel on multiple cores
 * @param name Human-readable name (optional)
 * @param id Static worker ID or WORKER_ID_INVALID to get a dynamic worker ID
 * @param coreMask Mask of cores on which the worker is allowed to run
 * @param body Worker body executed when a message is sent to the worker
 * @return Worker ID on success, WORKER_ID_INVALID on failure
 * @see WORKER_ID_INVALID
 */
static inline TWorkerId DeploySimpleParallelWorker(const char * name, TWorkerId id, int coreMask, TUserHandlerCallback body) {

    SWorkerConfig config = {
        .Name = name,
        .WorkerId = id,
        .CoreMask = coreMask,
        .Parallel = true,
        .WorkerBody = body,
    };
    return DeployWorker(&config);
}

/**
 * @brief Terminate a worker
 * @param workerId Worker ID of the worker to terminate or WORKER_ID_INVALID to terminate current worker
 * @note Once this function is called the worker will not receive any new messages, but is allowed to
 *       continue sending messages and using platform APIs until the current function returns
 * @see WORKER_ID_INVALID
 */
void TerminateWorker(TWorkerId workerId);

/**
 * @brief Access worker's shared private context data
 * @return Current worker's shared data handle (same for all cores)
 * @note Different cores correspond to different virtual address spaces so a pointer valid on one core
 *       need not be valid on another
 */
void * GetSharedData(void);

/**
 * @brief Set worker's shared private context data
 * @param data Private shared data
 * @note Different cores correspond to different virtual address spaces so a pointer valid on one core
 *       need not be valid on another
 */
void SetSharedData(void * data);

/**
 * @brief Access worker's per-core private context data
 * @return Current worker's private data handle (unique per core)
 */
void * GetLocalData(void);

/**
 * @brief Set worker's private context data on the current core
 * @param data Private local data
 */
void SetLocalData(void * data);

/**
 * @brief Get own worker ID
 * @return Current worker's worker ID
 */
TWorkerId GetOwnWorkerId(void);

/**
 * @brief For non-parallel workers, give a hint to the scheduler that atomic processing is done
 *        and the worker can be scheduled again in parallel
 */
void EndAtomicContext(void);

#ifdef __cplusplus
}
#endif

#endif /* PLATFORM_INTERFACE_MENABREA_WORKERS_H */
