#include <stddef.h>
#include <stdint.h>

#include <memory.h>

static void list_append(lru_page_t **, lru_page_t *);
static void list_remove(lru_page_t **, lru_page_t *);
static void refill(uint32_t);
static void reclaim(uint32_t);
static void balance(uint32_t);

lru_cache_t lru_cache __attribute__((section(".LRU_cache")));

/**
 * @brief Appends a node at the head of a LRU list.
 *
 * The new @p node becomes the first element, with its next pointer set to the
 * current list head (if any).
 *
 * @param list_head Current head of the list (may be NULL).
 * @param node Node to insert at the head of the list.
 */
static void list_append(lru_page_t **list_head, lru_page_t *node) {
    if (*list_head) (*list_head)->prev = node;

    node->next = *list_head;
    node->prev = NULL;
    
    *list_head = node;
}

/**
 * @brief Removes a node from the tail of a LRU list.
 *
 * This helper updates the neighbouring nodes' next/prev pointers so that
 * @p node is detached from the list identified by @p list.
 *
 * @param list Pointer to a node used as the list anchor.
 * @param node The list node to remove.
 */
static void list_remove(lru_page_t **list_tail, lru_page_t *node) {
    if (node->prev != NULL) node->prev->next = node->next;

    if (*list_tail == node) *list_tail = node->prev;

    if (node->next != NULL) node->next->prev = node->prev;
}

/**
 * @brief Refills the inactive list by scanning and demoting active pages.
 *
 * This function walks the active list from its tail and, for each page,
 * either:
 *  - moves it back to the head of the active list if it appears recently
 *    accessed (based on the accessed bit in the PTE), or
 *  - demotes it to the inactive list if not recently accessed.
 *
 * @param target Number of pages to demote from the active list.
 */
static void refill(uint32_t target) {
    lru_page_t *curr = lru_cache.active_tail;

    while (target > 0) {
        // Remove from the active list - the node will either be moved to the head the list or demoted
        list_remove(&lru_cache.active_tail, curr);

        if ((curr->virt_addr & 0x20) == 0x20) {
            // TODO: clear accessed bit

            // Move to head of active list
            list_append(&lru_cache.active_head, curr);
        }

        if ((curr->virt_addr & 0x20) == 0) {
            // Demote to inactive list
            list_append(&lru_cache.inactive_head, curr);

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

/**
 * @brief Reclaims or promotes pages from the inactive list.
 *
 * This function scans the inactive list from its tail while the current
 * reclaim @p mark is below the global @c high_watermark. For each page:
 *  - if the accessed bit is set, the page is promoted back to the active list;
 *  - if the accessed bit is clear, the page is considered reclaimable and
 *    @p mark is incremented (actual swap-out is still a TODO).
 *
 * @param mark Current reclaim progress indicator (e.g., number of freed pages).
 */
static void reclaim(uint32_t mark) {
    lru_page_t *curr = lru_cache.inactive_tail;

    while (mark < high_watermark) {
        // Remove from inactive list - it will either be promoted or reclaimed
        list_remove(&lru_cache.inactive_tail, curr);

        lru_cache.inactive--;

        if ((curr->virt_addr & 0x20) == 0x20) {
            // TODO: clear accessed bit

            // Promote to active list
            list_append(&lru_cache.active_head, curr);

            lru_cache.active++;
        }

        if ((curr->virt_addr & 0x20) == 0) {
            // TODO: move page to swap space

            mark++;
        }

        curr = curr->prev;
    }
}

/**
 * @brief Balances active and inactive LRU lists and reclaims memory.
 *
 * This function computes a target number of pages to demote from the active
 * list based on the relative sizes of the active and inactive sets, then:
 *
 *  - calls refill() to demote pages from the active list, and
 *  - calls reclaim() to reclaim or promote pages from the inactive list.
 *
 * @param mark Current reclaim progress indicator to pass to reclaim().
 */
static void balance(uint32_t mark) {
    // amount to refill = n * n_active / ((n_inactive + 1) * 2)
    uint32_t target = (lru_cache.active + lru_cache.inactive) * lru_cache.active / ((lru_cache.inactive + 1) * 2);

    refill(target);

    reclaim(mark);
}

/**
 * @brief Initializes LRU cache accounting and kswapd state.
 *
 * This function walks the active and inactive LRU lists to obtain their
 * current sizes and stores them in the global LRU cache structure. This
 * function is also the entry point for the kswapd thread.
 */
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

/**
 * @brief Adds a page to the LRU cache.
 *
 * This function allocates a new LRU page descriptor and inserts it into the
 * inactive list, incrementing the inactive page count.
 *
 * @param virt_addr Virtual address (or page table entry value) of the page.
 */
void lru_cache_add(uint32_t virt_addr) {
    lru_page_t *node = kmalloc(sizeof(lru_page_t));

    node->virt_addr = virt_addr;

    list_append(&lru_cache.inactive_head, node);

    lru_cache.inactive++;
}

/**
 * @brief Removes a page from the LRU cache.
 *
 * This function searches both the active and inactive lists for a page whose
 * recorded virtual address matches @p virt_addr. When found, the page is
 * unlinked from its list.
 *
 * @param virt_addr Virtual address of the page to remove from the cache.
 */
void lru_cache_del(uint32_t virt_addr) {
    lru_page_t *active_curr = lru_cache.active_head;

    while (active_curr != NULL) {
        if (active_curr->virt_addr == virt_addr) {
            list_remove(&lru_cache.active_tail, active_curr);

            // Free lru cache node
            kfree(active_curr, sizeof(lru_page_t));

            return;
        }

        active_curr = active_curr->next;
    }

    lru_page_t *inactive_curr = lru_cache.inactive_head;

    while (inactive_curr != NULL) {
        if (inactive_curr->virt_addr == virt_addr) {
            list_remove(&lru_cache.inactive_tail, inactive_curr);

            // Free lru cache node
            kfree(inactive_curr, sizeof(lru_page_t));

            return;
        }

        inactive_curr = inactive_curr->next;
    }
}