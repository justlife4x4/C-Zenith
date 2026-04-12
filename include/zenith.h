#ifndef ZENITH_H
#define ZENITH_H

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>

#ifdef _WIN32
#include <windows.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

// --- Configuration & Types ---

#ifdef _MSC_VER
#define ZENITH_THREAD_LOCAL __declspec(thread)
#define ZENITH_INLINE __inline
#else
#define ZENITH_THREAD_LOCAL _Thread_local
#define ZENITH_INLINE inline
#endif

typedef enum {
    ZENITH_OK = 0,
    ZENITH_ERROR = 1,
    ZENITH_PANIC = 2,
    ZENITH_CRASH_RECOVERED = 3
} zenith_status_t;

typedef void (*zenith_cleanup_fn)(void*);
typedef void (*zenith_log_fn)(const char* msg);

// --- Core API ---

void zenith_init(void);
void zenith_cleanup(void);
void zenith_set_logger(zenith_log_fn logger);

/**
 * @brief Retrieves global system-wide telemetry.
 * @param out_crashes Total number of hardware exceptions caught.
 * @param out_recoveries Total number of successful rollbacks.
 */
void zenith_get_telemetry(long* out_crashes, long* out_recoveries);

// --- Diagnostics ---

/**
 * @brief Prints a forensic report of the last crash encountered in the current thread.
 */
void zenith_print_crash_report(void);

// --- Transactional Memory & Resources ---

void* z_malloc(size_t size);
void z_free(void* ptr);

/**
 * @brief Registers a generic resource for cleanup.
 */
void z_defer(zenith_cleanup_fn fn, void* arg);

#ifdef _WIN32
/**
 * @brief Thread-safe mutex wrapper with automatic rollback protection.
 */
void z_mutex_lock(CRITICAL_SECTION* cs);
void z_mutex_unlock(CRITICAL_SECTION* cs);
#endif

// --- Guard Macros ---

#ifdef _MSC_VER
#define ZENITH_GUARD(status_ptr, block)                    \
    __try {                                                \
        *(status_ptr) = zenith_context_enter();            \
        if (*(status_ptr) == ZENITH_OK) {                  \
            block;                                         \
            zenith_context_exit();                         \
        }                                                  \
    } __except(zenith_msvc_handler(GetExceptionCode())) {  \
        *(status_ptr) = ZENITH_CRASH_RECOVERED;            \
    }
#else
#define ZENITH_GUARD(status_ptr, block)                    \
    do {                                                   \
        *(status_ptr) = zenith_context_enter();            \
        if (*(status_ptr) == ZENITH_OK) {                  \
            block;                                         \
            zenith_context_exit();                         \
        }                                                  \
    } while (0)
#endif

// --- Internal Engine ---

zenith_status_t zenith_context_enter(void);
void zenith_context_exit(void);
int zenith_msvc_handler(uint32_t code);

#ifdef __cplusplus
}
#endif

#endif // ZENITH_H
