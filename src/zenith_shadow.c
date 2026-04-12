#include "../include/zenith.h"
#include <stdio.h>
#include <stdlib.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/mman.h>
#include <unistd.h>
#endif

// Per-thread protected shadow page
static ZENITH_THREAD_LOCAL void* t_shadow_page = NULL;
static size_t g_page_size = 0;

void* zenith_shadow_init_thread(void) {
    if (t_shadow_page) return t_shadow_page;

#ifdef _WIN32
    if (g_page_size == 0) {
        SYSTEM_INFO si;
        GetSystemInfo(&si);
        g_page_size = si.dwPageSize;
    }
    // Allocate a PRIVATE page for this thread
    t_shadow_page = VirtualAlloc(NULL, g_page_size, MEM_COMMIT | MEM_RESERVE, PAGE_READONLY);
#else
    if (g_page_size == 0) g_page_size = sysconf(_SC_PAGESIZE);
    t_shadow_page = mmap(NULL, g_page_size, PROT_READ, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
#endif
    
    return t_shadow_page;
}

void zenith_shadow_lock(void) {
    if (!t_shadow_page) return;
    DWORD old;
#ifdef _WIN32
    VirtualProtect(t_shadow_page, g_page_size, PAGE_READONLY, &old);
#else
    mprotect(t_shadow_page, g_page_size, PROT_READ);
#endif
}

void zenith_shadow_unlock(void) {
    if (!t_shadow_page) return;
    DWORD old;
#ifdef _WIN32
    VirtualProtect(t_shadow_page, g_page_size, PAGE_READWRITE, &old);
#else
    mprotect(t_shadow_page, g_page_size, PROT_READ | PROT_WRITE);
#endif
}

void* zenith_shadow_get_base(void) {
    if (!t_shadow_page) return zenith_shadow_init_thread();
    return t_shadow_page;
}
