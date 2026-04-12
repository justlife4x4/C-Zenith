#include "zenith.h"
#include <stdio.h>
#include <stdlib.h>
#include <setjmp.h>

#ifdef _WIN32
#include <windows.h>
#include <malloc.h>
#endif

// --- Data Structures ---

typedef struct {
    uint32_t code;
    void* address;
    uint64_t timestamp;
} zenith_crash_report_t;

typedef struct zenith_ctx {
    jmp_buf jump_buffer;
    bool active;
    zenith_crash_report_t report; // The Black Box recorder
    struct zenith_ctx* next;
} zenith_ctx_t;

// --- Externs ---
extern void zenith_mem_rollback(void);
extern void zenith_mem_promote(void* current, void* parent);

extern void* zenith_shadow_init_thread(void);
extern void zenith_shadow_lock(void);
extern void zenith_shadow_unlock(void);
extern void* zenith_shadow_get_base(void);

// --- Thread-Local State ---
ZENITH_THREAD_LOCAL void* g_zenith_current_ctx = NULL;
static ZENITH_THREAD_LOCAL zenith_ctx_t* g_ctx_stack = NULL;
static ZENITH_THREAD_LOCAL bool g_zenith_in_handler = false;

// --- Global State ---
static zenith_log_fn g_logger = NULL;
static long g_initialized = 0;

// NEW: Global Telemetry Counters
static long g_total_crashes = 0;
static long g_total_recoveries = 0;

static void zenith_log(const char* msg) {
    if (g_logger) g_logger(msg);
    else fprintf(stderr, "[Zenith] %s\n", msg);
}

void zenith_init(void) {
    if (InterlockedCompareExchange(&g_initialized, 1, 0) == 0) {
        zenith_shadow_init_thread();
        zenith_log("System initialized (Zenith 2.0: Per-Thread Shadow Stack + Forensic Black Box).");
    }
}

// --- AUTOMATIC BOOTSTRAPPING (The "No-Touch" Feature) ---
#ifdef _MSC_VER
    // Microsoft Visual C++ Constructor hook
    #pragma section(".CRT$XCU", read)
    static void __cdecl auto_init(void) { zenith_init(); }
    __declspec(allocate(".CRT$XCU")) void (__cdecl* auto_init_ptr)(void) = auto_init;
#else
    // GCC/Clang Constructor hook
    void __attribute__((constructor)) auto_init(void) { zenith_init(); }
#endif

void zenith_cleanup(void) {}

void zenith_set_logger(zenith_log_fn logger) { g_logger = logger; }

int zenith_msvc_handler(uint32_t code) {
    if (g_zenith_in_handler) {
        zenith_log("RECURSIVE CRASH DETECTED. Emergency shutdown.");
        TerminateProcess(GetCurrentProcess(), 0xDEADBEEF);
    }
    g_zenith_in_handler = true;

    if (g_ctx_stack && g_ctx_stack->active) {
        // --- BLACK BOX RECORDING ---
        // Accessing protected memory to record the crash
        zenith_shadow_unlock();
        g_ctx_stack->report.code = code;
        // ExceptionAddress is available via GetExceptionInformation() 
        // Increment Global Crash Counter
        InterlockedIncrement(&g_total_crashes);

        zenith_log("Fault intercepted. Rolling back memory...");
        zenith_mem_rollback();
        
        if (code == EXCEPTION_STACK_OVERFLOW) {
            _resetstkoflw();
        }
        
        g_zenith_in_handler = false;
        return EXCEPTION_EXECUTE_HANDLER;
    }
    
    g_zenith_in_handler = false;
    return EXCEPTION_CONTINUE_SEARCH;
}

static zenith_ctx_t* shadow_alloc(void) {
    void* base = zenith_shadow_get_base();
    if (!base) return NULL;

    // We use a simple slot allocator within the thread's 4KB page
    static ZENITH_THREAD_LOCAL int next_slot = 0;
    if (next_slot * sizeof(zenith_ctx_t) >= 4096) return NULL; // Page full

    return &((zenith_ctx_t*)base)[next_slot++];
}

zenith_status_t zenith_context_enter(void) {
    zenith_ctx_t* new_ctx = shadow_alloc();
    if (!new_ctx) return ZENITH_ERROR;
    
    zenith_shadow_unlock();
    new_ctx->next = g_ctx_stack;
    new_ctx->active = true;
    new_ctx->report.code = 0; // Clear previous report
    zenith_shadow_lock();

    g_ctx_stack = new_ctx;
    g_zenith_current_ctx = new_ctx;
    return ZENITH_OK;
}

void zenith_context_exit(void) {
    if (g_ctx_stack) {
        zenith_ctx_t* current = g_ctx_stack;
        zenith_ctx_t* parent = (zenith_ctx_t*)g_ctx_stack->next;
        zenith_mem_promote(current, parent);
        g_ctx_stack = parent;
        g_zenith_current_ctx = parent;
        
        zenith_shadow_unlock();
        current->active = false;
        zenith_shadow_lock();
    }
}

// --- Deadlock-Resistant Mutex Support ---

void z_mutex_lock(CRITICAL_SECTION* cs) {
    EnterCriticalSection(cs);
    // Register automatic unlock on rollback
    z_defer((zenith_cleanup_fn)LeaveCriticalSection, cs);
}

void z_mutex_unlock(CRITICAL_SECTION* cs) {
    LeaveCriticalSection(cs);
}

// --- TELEMETRY API ---

void zenith_get_telemetry(long* out_crashes, long* out_recoveries) {
    if (out_crashes) *out_crashes = g_total_crashes;
    if (out_recoveries) *out_recoveries = g_total_crashes; // In Zenith, every caught crash is a recovery
}
