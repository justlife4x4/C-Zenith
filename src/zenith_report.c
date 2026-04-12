#include "zenith.h"
#include <stdio.h>
#include <setjmp.h>
#include <stdint.h>

// Shared internal structure from zenith_core.c
typedef struct {
    uint32_t code;
    void* address;
    uint64_t timestamp;
} zenith_crash_report_t;

typedef struct zenith_ctx {
    uint8_t dummy[sizeof(jmp_buf)]; // Must match zenith_core.c layout
    bool active;
    zenith_crash_report_t report;
    struct zenith_ctx* next;
} zenith_ctx_t;

extern ZENITH_THREAD_LOCAL void* g_zenith_current_ctx;

void zenith_print_crash_report(void) {
    if (!g_zenith_current_ctx) {
        printf("[Zenith Report] No active context to report from.\n");
        return;
    }

    zenith_ctx_t* ctx = (zenith_ctx_t*)g_zenith_current_ctx;
    
    printf("\n========================================\n");
    printf("     ZENITH 2.0 FORENSIC REPORT\n");
    printf("========================================\n");
    
    if (ctx->report.code == 0) {
        printf("Current Status: Stable (No faults recorded)\n");
    } else {
        printf("Last Fault Code: 0x%08X\n", ctx->report.code);
        
        const char* desc = "Unknown Hardware Exception";
        switch(ctx->report.code) {
            case 0xC0000005: desc = "Access Violation (Segmentation Fault)"; break;
            case 0xC0000094: desc = "Integer Division by Zero"; break;
            case 0xC00000FD: desc = "Stack Overflow"; break;
            case 0xC000001D: desc = "Illegal Instruction"; break;
        }
        printf("Description:     %s\n", desc);
        printf("Impact:          Rollback Successful (State Restored)\n");
    }
    printf("========================================\n\n");
}
