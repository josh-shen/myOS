#include <stddef.h>
#include <stdint.h>

#include <memory.h>

static void list_remove(lru_page_t *, lru_page_t *);
static void list_append(lru_page_t *, lru_page_t *);
static void refill(uint32_t);
static void reclaim(uint32_t);
static void balance(uint32_t);

lru_cache_t lru_cache __attribute__((section(".LRU_cache")));

static void list_remove(lru_page_t *list, lru_page_t *node) {
    if (node->prev != NULL) node->prev->next = node->next;

    if (list == node) list = node->prev;

    if (node->next != NULL) node->next->prev = node->prev;
}

static void list_append(lru_page_t *list, lru_page_t *node) {
    if (list) list->prev = node;

    node->next = list;
    node->prev = NULL;
}

static void refill(uint32_t target) {
    lru_page_t *curr = lru_cache.active_tail;

    while (target > 0) {
        // Remove from the active list - the node will either be moved to the head the list or demoted
        list_remove(lru_cache.active_tail, curr);

        if ((curr->virt_addr & 0x20) == 0x20) {
            // TODO: clear accessed bit

            // Move to head of active list
            list_append(lru_cache.active_head, curr);
        }

        if ((curr->virt_addr & 0x20) == 0) {
            // Demote to inactive list
            list_append(lru_cache.inactive_head, curr);

            /*
            TODO: clear present bit

            this will trigger a page fault when accessing this page
            the page fault handler should set the present bit back to 1, 
            and move the page to the head of the active list
            */

            lru_cache.active--;
            lru_cache.inactive++;

            target--;
        }

        curr = curr->prev;
    }
}

static void reclaim(uint32_t mark) {
    lru_page_t *curr = lru_cache.inactive_tail;

    while (mark < high_watermark) {
        // Remove from inactive list - it will either be promoted or reclaimed
        list_remove(lru_cache.inactive_tail, curr);

        lru_cache.inactive--;

        if ((curr->virt_addr & 0x20) == 0x20) {
            // TODO: clear accessed bit

            // Promote to active list
            list_append(lru_cache.active_head, curr);

            lru_cache.active++;
        }

        if ((curr->virt_addr & 0x20) == 0) {
            // TODO: move page to swap space

            mark++;
        }

        curr = curr->prev;
    }
}

static void balance(uint32_t mark) {
    // amount to refill = n * n_active / ((n_inactive + 1) * 2)
    uint32_t target = (lru_cache.active + lru_cache.inactive) * lru_cache.active / ((lru_cache.inactive + 1) * 2);
    
    refill(target);

    reclaim(mark);
}

void kswapd_init() {
    // Initialize LRU cache with active and inactive counts
    lru_page_t *active_curr = lru_cache.active_head;
    uint32_t active_count = 0;
    while (active_curr != NULL) {
        active_count++;

        active_curr = active_curr->next;
    }

    lru_page_t *inactive_curr = lru_cache.inactive_head;
    uint32_t inactive_count = 0;
    while(inactive_curr != NULL) {
        inactive_count++;

        inactive_curr = inactive_curr->next;
    }

    lru_cache.active = active_count;
    lru_cache.inactive = inactive_count;

    /*
    thread loop

    wake up when number of free pages decreases to below low_watermark
    call balance

    sleep when high_watermark has been reached
    */
}

void lru_cache_add(uint32_t virt_addr) {
    lru_page_t *node = (lru_page_t *)kmalloc(sizeof(lru_page_t));

    list_append(lru_cache.inactive_head, node);

    lru_cache.inactive++;
}

void lru_cache_del(uint32_t virt_addr) {
    lru_page_t *active_curr = lru_cache.active_head;

    while (active_curr != NULL) {
        if (active_curr->virt_addr == virt_addr) {
            list_remove(lru_cache.active_tail, active_curr);

            return;
        }

        active_curr = active_curr->next;
    }

    lru_page_t *inactive_curr = lru_cache.inactive_head;

    while (inactive_curr != NULL) {
        if (inactive_curr->virt_addr == virt_addr) {
            list_remove(lru_cache.inactive_tail, inactive_curr);

            return;
        }

        inactive_curr = inactive_curr->next;
    }
}