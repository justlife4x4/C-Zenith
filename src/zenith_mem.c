#include "zenith.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#define ZENITH_META_MAGIC 0x5A454E4954484D41ULL

typedef struct z_resource {
    void* ptr;
    zenith_cleanup_fn cleanup;
    void* ctx_id;           // Context ownership
    struct z_resource* next;
} z_resource_t;

typedef struct zenith_meta {
    uint64_t canary;
    void* ctx_id;           // Context ownership
    struct zenith_meta* next;
    size_t size;
} zenith_meta_t;

extern ZENITH_THREAD_LOCAL void* g_zenith_current_ctx;

static ZENITH_THREAD_LOCAL zenith_meta_t* g_alloc_head = NULL;
static ZENITH_THREAD_LOCAL z_resource_t* g_resource_head = NULL;

void* z_malloc(size_t size) {
    size_t total_size = sizeof(zenith_meta_t) + size;
    zenith_meta_t* meta = (zenith_meta_t*)malloc(total_size);
    if (!meta) return NULL;

    meta->canary = ZENITH_META_MAGIC;
    meta->ctx_id = g_zenith_current_ctx;
    meta->size = size;
    
    meta->next = g_alloc_head;
    g_alloc_head = meta;

    return (void*)(meta + 1);
}

void z_free(void* ptr) {
    if (!ptr) return;
    zenith_meta_t* meta = (zenith_meta_t*)ptr - 1;
    if (meta->canary != ZENITH_META_MAGIC) {
        fprintf(stderr, "[Zenith Panic] Corruption detected in z_free!\n");
        abort();
    }

    zenith_meta_t** curr = &g_alloc_head;
    while (*curr) {
        if (*curr == meta) {
            *curr = meta->next;
            break;
        }
        curr = &((*curr)->next);
    }
    free(meta);
}

void z_defer(zenith_cleanup_fn fn, void* arg) {
    if (!fn) return;
    z_resource_t* res = (z_resource_t*)malloc(sizeof(z_resource_t));
    if (!res) return;
    res->ptr = arg;
    res->cleanup = fn;
    res->ctx_id = g_zenith_current_ctx; // Track which context owns this resource
    res->next = g_resource_head;
    g_resource_head = res;
}

void zenith_mem_rollback(void) {
    void* target_ctx = g_zenith_current_ctx;
    if (!target_ctx) return;

    // 1. Rollback resources for THIS context only
    z_resource_t** r_curr = &g_resource_head;
    while (*r_curr) {
        if ((*r_curr)->ctx_id == target_ctx) {
            z_resource_t* to_free = *r_curr;
            *r_curr = to_free->next;
            
            if (to_free->cleanup) to_free->cleanup(to_free->ptr);
            free(to_free);
        } else {
            r_curr = &((*r_curr)->next);
        }
    }

    // 2. Rollback memory for THIS context only
    zenith_meta_t** m_curr = &g_alloc_head;
    while (*m_curr) {
        if ((*m_curr)->ctx_id == target_ctx) {
            zenith_meta_t* to_free = *m_curr;
            *m_curr = to_free->next;
            
            if (to_free->canary == ZENITH_META_MAGIC) free(to_free);
            else abort();
        } else {
            m_curr = &((*m_curr)->next);
        }
    }
}

void zenith_mem_promote(void* current, void* parent) {
    if (!current) return;

    // 1. Promote memory
    zenith_meta_t* m_curr = g_alloc_head;
    while (m_curr) {
        if (m_curr->ctx_id == current) {
            m_curr->ctx_id = parent;
        }
        m_curr = m_curr->next;
    }

    // 2. Promote resources
    z_resource_t* r_curr = g_resource_head;
    while (r_curr) {
        if (r_curr->ctx_id == current) {
            r_curr->ctx_id = parent;
        }
        r_curr = r_curr->next;
    }
}
