#ifndef _WIN32

#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <setjmp.h>
#include <unistd.h>

// Forward declarations from zenith_mem.c
extern void zenith_mem_rollback(void);
extern void zenith_mem_promote(void* current, void* parent);

// TLS State
extern ZENITH_THREAD_LOCAL void* g_zenith_current_ctx;
static __thread bool g_in_handler = false;

typedef struct zenith_ctx_posix {
    sigjmp_buf jump_buffer;
    bool active;
    struct zenith_ctx_posix* next;
} zenith_ctx_posix_t;

static __thread zenith_ctx_posix_t* g_posix_stack = NULL;

void zenith_posix_handler(int sig, siginfo_t* info, void* ucontext) {
    if (g_in_handler) {
        fprintf(stderr, "[Zenith] Fatal recursive fault. Terminanting.\n");
        _exit(EXIT_FAILURE);
    }

    g_in_handler = true;

    if (g_posix_stack && g_posix_stack->active) {
        zenith_mem_rollback();
        g_in_handler = false;
        siglongjmp(g_posix_stack->jump_buffer, ZENITH_CRASH_RECOVERED);
    }

    g_in_handler = false;
    // If not handled, restore original handler or exit
    _exit(EXIT_FAILURE);
}

void zenith_init_posix(void) {
    // 1. Setup Signal Alternate Stack (for Stack Overflow recovery)
    stack_t ss;
    ss.ss_sp = malloc(SIGSTKSZ);
    ss.ss_size = SIGSTKSZ;
    ss.ss_flags = 0;
    if (sigaltstack(&ss, NULL) == -1) {
        perror("sigaltstack");
    }

    // 2. Register Signal Handlers
    struct sigaction sa;
    sa.sa_sigaction = zenith_posix_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_SIGINFO | SA_ONSTACK;

    sigaction(SIGSEGV, &sa, NULL);
    sigaction(SIGFPE, &sa, NULL);
    sigaction(SIGILL, &sa, NULL);
    sigaction(SIGBUS, &sa, NULL);
}

// POSIX implementation of context enter/exit
// (To be used when _MSC_VER is not defined)

zenith_status_t zenith_context_enter(void) {
    // Logic similar to Windows but using sigsetjmp
    // For V2 Universal, we'd use the shadow stack pool here too.
    return ZENITH_OK; 
}

#endif
