
#ifndef PLATFORM_INTERFACE_MENABREA_COMMON_H
#define PLATFORM_INTERFACE_MENABREA_COMMON_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <event_machine/platform/env/environment.h>

typedef uint64_t u64;  /**< 64-bit unsigned type */
typedef uint32_t u32;  /**< 32-bit unsigned type */
typedef uint16_t u16;  /**< 16-bit unsigned type */
typedef uint8_t u8;    /**< 8-bit unsigned type */

typedef int64_t i64;   /**< 64-bit signed type */
typedef int32_t i32;   /**< 32-bit signed type */
typedef int16_t i16;   /**< 16-bit signed type */
typedef int8_t i8;     /**< 8-bit signed type */

/** @brief 32-bit atomic counter type */
typedef env_atomic32_t TAtomic32;

/**
 * @brief Initialize a 32-bit atomic counter
 * @param atomic Atomic counter handle
 */
static inline void Atomic32Init(TAtomic32 * atomic)                             { env_atomic32_init(atomic); }

/**
 * @brief Atomically get the current value of a 32-bit atomic counter
 * @param atomic Atomic counter handle
 * @return Current value of the counter
 */
static inline u32 Atomic32Get(TAtomic32 * atomic)                               { return env_atomic32_get(atomic); }

/**
 * @brief Atomically set the value of a 32-bit atomic counter
 * @param atomic Atomic counter handle
 * @param value Value to set the counter to
 */
static inline void Atomic32Set(TAtomic32 * atomic, u32 value)                   { env_atomic32_set(atomic, value); }

/**
 * @brief Atomically add a value to a 32-bit atomic counter
 * @param atomic Atomic counter handle
 * @param summand Value to be added to the atomic counter
 */
static inline void Atomic32Add(TAtomic32 * atomic, u32 summand)                 { env_atomic32_add(atomic, summand); }

/**
 * @brief Atomically add a value to a 32-bit atomic counter and return the result
 * @param atomic Atomic counter handle
 * @param summand Value to be added to the atomic counter
 * @return Result of the addition
 * @note This is logically equivalent to Atomic32Add(atomic, summand) followed by Atomic32Get(atomic),
 *       except the two are executed as a single atomic unit
 */
static inline u32 Atomic32AddReturn(TAtomic32 * atomic, u32 summand)            { return env_atomic32_add_return(atomic, summand); }

/**
 * @brief Atomically add a value to a 32-bit atomic counter and return the value of the counter from before the addition
 * @param atomic Atomic counter handle
 * @param summand Value to be added to the atomic counter
 * @return Value of the atomic counter before the addition
 * @note This is logically equivalent to Atomic32Get(atomic) followed by Atomic32Add(atomic, summand),
 *       except the two are executed as a single atomic unit
 */
static inline u32 Atomic32ReturnAdd(TAtomic32 * atomic, u32 summand)            { return env_atomic32_return_add(atomic, summand); }

/**
 * @brief Atomically subtract a value from a 32-bit atomic counter
 * @param atomic Atomic counter handle
 * @param subtrahend Value to be subtracted from the atomic counter
 */
static inline void Atomic32Sub(TAtomic32 * atomic, u32 subtrahend)              { env_atomic32_sub(atomic, subtrahend); }

/**
 * @brief Atomically subtract a value from a 32-bit atomic counter and return the result
 * @param atomic Atomic counter handle
 * @param subtrahend Value to be subtracted from the atomic counter
 * @return Result of the subtraction
 * @note This is logically equivalent to Atomic32Sub(atomic, subtrahend) followed by Atomic32Get(atomic),
 *       except the two are executed as a single atomic unit
 */
static inline u32 Atomic32SubReturn(TAtomic32 * atomic, u32 subtrahend)         { return env_atomic32_sub_return(atomic, subtrahend); }

/**
 * @brief Atomically subtract a value from a 32-bit atomic counter and return the value of the counter from before the subtraction
 * @param atomic Atomic counter handle
 * @param subtrahend Value to be subtracted from the atomic counter
 * @return Value of the atomic counter before the subtraction
 * @note This is logically equivalent to Atomic32Get(atomic) followed by Atomic32Sub(atomic, subtrahend),
 *       except the two are executed as a single atomic unit
 */
static inline u32 Atomic32ReturnSub(TAtomic32 * atomic, u32 subtrahend)         { return env_atomic32_return_sub(atomic, subtrahend); }

/**
 * @brief Atomically increment a 32-bit atomic counter
 * @param atomic Atomic counter handle
 * @note This is logically equivalent to Atomic32Add(atomic, 1)
 */
static inline void Atomic32Inc(TAtomic32 * atomic)                              { env_atomic32_inc(atomic); }

/**
 * @brief Atomically decrement a 32-bit atomic counter
 * @param atomic Atomic counter handle
 * @note This is logically equivalent to Atomic32Sub(atomic, 1)
 */
static inline void Atomic32Dec(TAtomic32 * atomic)                              { env_atomic32_dec(atomic); }

/**
 * @brief Do the atomic "compare and exchange" operation on a 32-bit atomic counter
 * @param atomic Atomic counter handle
 * @param expected Value to test (compare) the counter against
 * @param desired Value to set the counter to if comparison yields true, i.e. if value of the atomic counter is equal to expected
 * @return A non-zero value if comparison returned true and the atomic counter was set to desired, zero otherwise
 * @note This is logically equivalent to if (expected == Atomic32Get(atomic)) { Atomic32Set(atomic, desired); }, but is executed
 *       atomically as a single unit
 */
static inline int Atomic32CmpSet(TAtomic32 * atomic, u32 expected, u32 desired) { return env_atomic32_cmpset(atomic, expected, desired); }

/**
 * @brief Atomically get the value of a 32-bit atomic counter and set a new one
 * @param atomic Atomic counter handle
 * @param newValue value to set the counter to
 * @return Previous value of the atomic counter
 * @note This is logically equivalent to Atomic32Get(atomic) followed by Atomic32Set(atomic, newValue),
 *       except the two are executed as a single atomic unit
 */
static inline u32 Atomic32Exchange(TAtomic32 * atomic, u32 newValue)            { return env_atomic32_exchange(atomic, newValue); }

/** @brief 64-bit atomic counter type */
typedef env_atomic64_t TAtomic64;

/**
 * @brief Initialize a 64-bit atomic counter
 * @param atomic Atomic counter handle
 */
static inline void Atomic64Init(TAtomic64 * atomic)                             { env_atomic64_init(atomic); }

/**
 * @brief Atomically get the current value of a 64-bit atomic counter
 * @param atomic Atomic counter handle
 * @return Current value of the counter
 */
static inline u64 Atomic64Get(TAtomic64 * atomic)                               { return env_atomic64_get(atomic); }

/**
 * @brief Atomically set the value of a 64-bit atomic counter
 * @param atomic Atomic counter handle
 * @param value Value to set the counter to
 */
static inline void Atomic64Set(TAtomic64 * atomic, u64 value)                   { env_atomic64_set(atomic, value); }

/**
 * @brief Atomically add a value to a 64-bit atomic counter
 * @param atomic Atomic counter handle
 * @param summand Value to be added to the atomic counter
 */
static inline void Atomic64Add(TAtomic64 * atomic, u64 summand)                 { env_atomic64_add(atomic, summand); }

/**
 * @brief Atomically add a value to a 64-bit atomic counter and return the result
 * @param atomic Atomic counter handle
 * @param summand Value to be added to the atomic counter
 * @return Result of the addition
 * @note This is logically equivalent to Atomic64Add(atomic, summand) followed by Atomic64Get(atomic),
 *       except the two are executed as a single atomic unit
 */
static inline u64 Atomic64AddReturn(TAtomic64 * atomic, u64 summand)            { return env_atomic64_add_return(atomic, summand); }

/**
 * @brief Atomically add a value to a 64-bit atomic counter and return the value of the counter from before the addition
 * @param atomic Atomic counter handle
 * @param summand Value to be added to the atomic counter
 * @return Value of the atomic counter before the addition
 * @note This is logically equivalent to Atomic64Get(atomic) followed by Atomic64Add(atomic, summand),
 *       except the two are executed as a single atomic unit
 */
static inline u64 Atomic64ReturnAdd(TAtomic64 * atomic, u64 summand)            { return env_atomic64_return_add(atomic, summand); }

/**
 * @brief Atomically subtract a value from a 64-bit atomic counter
 * @param atomic Atomic counter handle
 * @param subtrahend Value to be subtracted from the atomic counter
 */
static inline void Atomic64Sub(TAtomic64 * atomic, u64 subtrahend)              { env_atomic64_sub(atomic, subtrahend); }

/**
 * @brief Atomically subtract a value from a 64-bit atomic counter and return the result
 * @param atomic Atomic counter handle
 * @param subtrahend Value to be subtracted from the atomic counter
 * @return Result of the subtraction
 * @note This is logically equivalent to Atomic64Sub(atomic, subtrahend) followed by Atomic64Get(atomic),
 *       except the two are executed as a single atomic unit
 */
static inline u64 Atomic64SubReturn(TAtomic64 * atomic, u64 subtrahend)         { return env_atomic64_sub_return(atomic, subtrahend); }

/**
 * @brief Atomically subtract a value from a 64-bit atomic counter and return the value of the counter from before the subtraction
 * @param atomic Atomic counter handle
 * @param subtrahend Value to be subtracted from the atomic counter
 * @return Value of the atomic counter before the subtraction
 * @note This is logically equivalent to Atomic64Get(atomic) followed by Atomic64Sub(atomic, subtrahend),
 *       except the two are executed as a single atomic unit
 */
static inline u64 Atomic64ReturnSub(TAtomic64 * atomic, u64 subtrahend)         { return env_atomic64_return_sub(atomic, subtrahend); }

/**
 * @brief Atomically increment a 64-bit atomic counter
 * @param atomic Atomic counter handle
 * @note This is logically equivalent to Atomic64Add(atomic, 1)
 */
static inline void Atomic64Inc(TAtomic64 * atomic)                              { env_atomic64_inc(atomic); }

/**
 * @brief Atomically decrement a 64-bit atomic counter
 * @param atomic Atomic counter handle
 * @note This is logically equivalent to Atomic64Sub(atomic, 1)
 */
static inline void Atomic64Dec(TAtomic64 * atomic)                              { env_atomic64_dec(atomic); }

/**
 * @brief Do the atomic "compare and exchange" operation on a 64-bit atomic counter
 * @param atomic Atomic counter handle
 * @param expected Value to test (compare) the counter against
 * @param desired Value to set the counter to if comparison yields true, i.e. if value of the atomic counter is equal to expected
 * @return A non-zero value if comparison returned true and the atomic counter was set to desired, zero otherwise
 * @note This is logically equivalent to if (expected == Atomic64Get(atomic)) { Atomic64Set(atomic, desired); }, but is executed
 *       atomically as a single unit
 */
static inline int Atomic64CmpSet(TAtomic64 * atomic, u64 expected, u64 desired) { return env_atomic64_cmpset(atomic, expected, desired); }

/**
 * @brief Atomically get the value of a 64-bit atomic counter and set a new one
 * @param atomic Atomic counter handle
 * @param newValue value to set the counter to
 * @return Previous value of the atomic counter
 * @note This is logically equivalent to Atomic64Get(atomic) followed by Atomic64Set(atomic, newValue),
 *       except the two are executed as a single atomic unit
 */
static inline u64 Atomic64Exchange(TAtomic64 * atomic, u64 newValue)            { return env_atomic64_exchange(atomic, newValue); }

/** @brief Spinlock type */
typedef env_spinlock_t TSpinlock;

/**
 * @brief Initialize a spinlock
 * @param lock Spinlock handle
 */
static inline void SpinlockInit(TSpinlock * lock)                               { env_spinlock_init(lock); }

/**
 * @brief Acquire (lock) a spinlock
 * @param lock Spinlock handle
 */
static inline void SpinlockAcquire(TSpinlock * lock)                            { env_spinlock_lock(lock); }

/**
 * @brief Release (unlock) a spinlock
 * @param lock Spinlock handle
 */
static inline void SpinlockRelease(TSpinlock * lock)                            { env_spinlock_unlock(lock); }

#define __APPLICATION_GLOBAL_INIT_NAME  ApplicationGlobalInit
#define __APPLICATION_LOCAL_INIT_NAME   ApplicationLocalInit
#define __APPLICATION_LOCAL_EXIT_NAME   ApplicationLocalExit
#define __APPLICATION_GLOBAL_EXIT_NAME  ApplicationGlobalExit

#define __MACRO_STRINGIFY_IMPL(x)       #x
#define __MACRO_STRINGIFY(x)            __MACRO_STRINGIFY_IMPL(x)

#define __APPLICATION_GLOBAL_INIT_SYM   __MACRO_STRINGIFY(__APPLICATION_GLOBAL_INIT_NAME)
#define __APPLICATION_LOCAL_INIT_SYM    __MACRO_STRINGIFY(__APPLICATION_LOCAL_INIT_NAME)
#define __APPLICATION_LOCAL_EXIT_SYM    __MACRO_STRINGIFY(__APPLICATION_LOCAL_EXIT_NAME)
#define __APPLICATION_GLOBAL_EXIT_SYM   __MACRO_STRINGIFY(__APPLICATION_GLOBAL_EXIT_NAME)

#ifdef __cplusplus
    #define __APPLICATION_CALLBACK_PREFIX  extern "C" __attribute__((visibility("default")))
#else
    #define __APPLICATION_CALLBACK_PREFIX  __attribute__((visibility("default")))
#endif

/**
 * @brief Definition of the ApplicationGlobalInit symbol
 * @note The application library's global init function will be called by the platform during startup before forking
 *       the event dispatchers. It gives the application an opportunity to do global initialization, e.g. set up shared memory
 *       in a way that allows passing pointers directly between processes by ensuring identical address space layouts in all
 *       processes. All platform APIs can already be used at this point (except where explicitly noted).
 */
#define APPLICATION_GLOBAL_INIT()       __APPLICATION_CALLBACK_PREFIX void __APPLICATION_GLOBAL_INIT_NAME(void)

/**
 * @brief Definition of the ApplicationLocalInit symbol
 * @note The application library's local init will be called once in each dispatcher process during the platform startup. The index of
 *       the core on which execution takes place will be passed to the function via its argument.
 */
#define APPLICATION_LOCAL_INIT(__core)  __APPLICATION_CALLBACK_PREFIX void __APPLICATION_LOCAL_INIT_NAME(int __core)

/**
 * @brief Definition of the ApplicationLocalExit symbol
 * @note The application library's local exit will be called once in each dispatcher process during the platform shutdown. The index of
 *       the core on which execution takes place will be passed to the function via its argument.
 */
#define APPLICATION_LOCAL_EXIT(__core)  __APPLICATION_CALLBACK_PREFIX void __APPLICATION_LOCAL_EXIT_NAME(int __core)

/**
 * @brief Definition of the ApplicationGlobalInit symbol
 * @note The application library's global exit will be called by the platform during shutdown after local exits complete in all dispatchers
 *       ("EM cores"). All platform APIs can still be used at this point (except where explicitly noted).
 */
#define APPLICATION_GLOBAL_EXIT()       __APPLICATION_CALLBACK_PREFIX void __APPLICATION_GLOBAL_EXIT_NAME(void)

#endif /* PLATFORM_INTERFACE_MENABREA_COMMON_H */
