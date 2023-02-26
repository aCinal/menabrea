
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

typedef env_atomic32_t TAtomic32;
static inline void Atomic32Init(TAtomic32 * atomic)                             { env_atomic32_init(atomic); }
static inline u32 Atomic32Get(TAtomic32 * atomic)                               { return env_atomic32_get(atomic); }
static inline void Atomic32Set(TAtomic32 * atomic, u32 value)                   { env_atomic32_set(atomic, value); }
static inline void Atomic32Add(TAtomic32 * atomic, u32 summand)                 { env_atomic32_add(atomic, summand); }
static inline u32 Atomic32AddReturn(TAtomic32 * atomic, u32 summand)            { return env_atomic32_add_return(atomic, summand); }
static inline u32 Atomic32ReturnAdd(TAtomic32 * atomic, u32 summand)            { return env_atomic32_return_add(atomic, summand); }
static inline void Atomic32Inc(TAtomic32 * atomic)                              { env_atomic32_inc(atomic); }
static inline void Atomic32Dec(TAtomic32 * atomic)                              { env_atomic32_dec(atomic); }
static inline void Atomic32Sub(TAtomic32 * atomic, u32 subtrahend)              { env_atomic32_sub(atomic, subtrahend); }
static inline u32 Atomic32SubReturn(TAtomic32 * atomic, u32 subtrahend)         { return env_atomic32_sub_return(atomic, subtrahend); }
static inline u32 Atomic32ReturnSub(TAtomic32 * atomic, u32 subtrahend)         { return env_atomic32_return_sub(atomic, subtrahend); }
static inline int Atomic32CmpSet(TAtomic32 * atomic, u32 expected, u32 desired) { return env_atomic32_cmpset(atomic, expected, desired); }
static inline u32 Atomic32Exchange(TAtomic32 * atomic, u32 newValue)            { return env_atomic32_exchange(atomic, newValue); }

typedef env_atomic64_t TAtomic64;
static inline void Atomic64Init(TAtomic64 * atomic)                             { env_atomic64_init(atomic); }
static inline u64 Atomic64Get(TAtomic64 * atomic)                               { return env_atomic64_get(atomic); }
static inline void Atomic64Set(TAtomic64 * atomic, u64 value)                   { env_atomic64_set(atomic, value); }
static inline void Atomic64Add(TAtomic64 * atomic, u64 summand)                 { env_atomic64_add(atomic, summand); }
static inline u64 Atomic64AddReturn(TAtomic64 * atomic, u64 summand)            { return env_atomic64_add_return(atomic, summand); }
static inline u64 Atomic64ReturnAdd(TAtomic64 * atomic, u64 summand)            { return env_atomic64_return_add(atomic, summand); }
static inline void Atomic64Inc(TAtomic64 * atomic)                              { env_atomic64_inc(atomic); }
static inline void Atomic64Dec(TAtomic64 * atomic)                              { env_atomic64_dec(atomic); }
static inline void Atomic64Sub(TAtomic64 * atomic, u64 subtrahend)              { env_atomic64_sub(atomic, subtrahend); }
static inline u64 Atomic64SubReturn(TAtomic64 * atomic, u64 subtrahend)         { return env_atomic64_sub_return(atomic, subtrahend); }
static inline u64 Atomic64ReturnSub(TAtomic64 * atomic, u64 subtrahend)         { return env_atomic64_return_sub(atomic, subtrahend); }
static inline int Atomic64CmpSet(TAtomic64 * atomic, u64 expected, u64 desired) { return env_atomic64_cmpset(atomic, expected, desired); }
static inline u64 Atomic64Exchange(TAtomic64 * atomic, u64 newValue)            { return env_atomic64_exchange(atomic, newValue); }

typedef env_spinlock_t TSpinlock;
static inline void SpinlockInit(TSpinlock * lock)                               { env_spinlock_init(lock); }
static inline void SpinlockAcquire(TSpinlock * lock)                            { env_spinlock_lock(lock); }
static inline void SpinlockRelease(TSpinlock * lock)                            { env_spinlock_unlock(lock); }

/* TODO: Add Doxygen comments in this file */

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

#define APPLICATION_GLOBAL_INIT()       extern "C" __attribute__((visibility("default"))) void __APPLICATION_GLOBAL_INIT_NAME(void)
#define APPLICATION_LOCAL_INIT(__core)  extern "C" __attribute__((visibility("default"))) void __APPLICATION_LOCAL_INIT_NAME(int __core)
#define APPLICATION_LOCAL_EXIT(__core)  extern "C" __attribute__((visibility("default"))) void __APPLICATION_LOCAL_EXIT_NAME(int __core)
#define APPLICATION_GLOBAL_EXIT()       extern "C" __attribute__((visibility("default"))) void __APPLICATION_GLOBAL_EXIT_NAME(void)

#endif /* PLATFORM_INTERFACE_MENABREA_COMMON_H */
