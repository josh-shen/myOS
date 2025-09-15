#include <stdint.h>
#include <stdio.h>

#include <memory.h>

static cache_t *cache_chain = NULL;
static cache_t *cache_cache = NULL;
static cache_t *slab_cache = NULL;

static void *object_alloc(cache_t *cache) {
    if (cache->slabs_empty == NULL && cache->slabs_partial == NULL) {
        cache_grow(cache);
    } 
    if (cache->slabs_partial != NULL) {
        // allocate from partial slab
        slab_t *target_slab = cache->slabs_partial;
        object_t *obj = target_slab->head;
        target_slab->head = obj->next;
        target_slab->inuse++;
        if (target_slab->inuse == cache->num) {
            // move slab to full list
            cache->slabs_partial = NULL;
            cache->slabs_full = target_slab;
        }
        return obj;
    } else if (cache->slabs_empty != NULL) {
        // allocate from empty slab
        slab_t *target_slab = cache->slabs_empty;
        object_t *obj = target_slab->head;
        target_slab->head = obj->next;
        target_slab->inuse++;
        if (target_slab->inuse == cache->num) {
            // move slab to full list
            cache->slabs_empty = NULL;
            cache->slabs_full = target_slab;
        } else {
            // move slab to partial list
            cache->slabs_empty = NULL;
            cache->slabs_partial = target_slab;
        }
        return obj;
    }
}

static void slab_cache_grow() {
    uint32_t addr = vmm_malloc(4096);
    slab_t *new_slab = (slab_t *)addr;
    new_slab->head = NULL;
    new_slab->inuse = 0;

    // add to empty list of slab_cache
    slab_cache->slabs_empty = new_slab;

    // link objects within slab
    uint32_t curr_addr = addr + sizeof(slab_t);
    object_t *curr_obj = (object_t *)curr_addr;
    slab_cache->slabs_empty->head = curr_obj;
    while (curr_addr + slab_cache->objsize <= addr + 4096) {
        curr_obj->next = (object_t *)(curr_addr + slab_cache->objsize);
        curr_addr += slab_cache->objsize;
        curr_obj = curr_obj->next;
    }
}

static void cache_grow(cache_t *cache) {
   if (slab_cache->slabs_empty == NULL && slab_cache->slabs_partial == NULL) {
       slab_cache_grow();
   }
   
   // allocate memory for slab struct from slab_cache
   object_t *slab_obj;
   
   if (slab_cache->slabs_partial != NULL) {
       slab_t *slab_slab = slab_cache->slabs_partial;
       slab_slab->head = slab_obj->next;
       slab_slab->inuse++;
       if (slab_slab->inuse == slab_cache->num) {
           // move slab to full list
           slab_cache->slabs_partial = NULL;
           slab_cache->slabs_full = slab_slab;
       }
       slab_obj = slab_slab->head;
   } else if (slab_cache->slabs_empty != NULL) {
       slab_t *slab_slab = slab_cache->slabs_empty;
       slab_slab->head = slab_obj->next;
       slab_slab->inuse++;
       if (slab_slab->inuse == slab_cache->num) {
           // move slab to full list
           slab_cache->slabs_empty = NULL;
           slab_cache->slabs_full = slab_slab;
       } else {
           // move slab to partial list
           slab_cache->slabs_empty = NULL;
           slab_cache->slabs_partial = slab_slab;
       }
       slab_obj = slab_slab->head;
   }

    // initialize new slab
    slab_t *new_slab = (slab_t *)slab_obj;
    new_slab->inuse = 0;
    uint32_t slab_mem = vmm_malloc(4096);
    new_slab->head = (object_t *)slab_mem;
    // link objects within slab
    uint32_t curr_addr = slab_mem;
    object_t *curr_obj = (object_t *)new_slab->head;
    while (curr_addr + cache->objsize <= 4096) {
        curr_obj->next = (object_t *)(curr_addr + cache->objsize);
        curr_addr += cache->objsize;
        curr_obj = curr_obj->next;
    }
}

static void cache_create() {
   if (cache_cache->slabs_empty == NULL && cache_cache->slabs_partial == NULL) {
       cache_grow(cache_cache);
   }

   object_t *cache_obj;

   if (cache_cache->slabs_partial != NULL) {
       // allocate from partial slab
       slab_t *cache_slab = cache_cache->slabs_partial;
       cache_obj = cache_slab->head;
       cache_slab->head = cache_obj->next;
       cache_slab->inuse++;
       if (cache_slab->inuse == cache_cache->num) {
           // move slab to full list
           cache_cache->slabs_partial = NULL;
           cache_cache->slabs_full = cache_slab;
       }
   } else if (cache_cache->slabs_empty != NULL) {
       // allocate from empty slab
       slab_t *cache_slab = cache_cache->slabs_empty;
       cache_obj = cache_slab->head;
       cache_slab->head = cache_obj->next;
       cache_slab->inuse++;
       if (cache_slab->inuse == cache_cache->num) {
           // move slab to full list
           cache_cache->slabs_empty = NULL;
           cache_cache->slabs_full = cache_slab;
       } else {
           // move slab to partial list
           cache_cache->slabs_empty = NULL;
           cache_cache->slabs_partial = cache_slab;
       }
   }

    cache_t *new_cache = (cache_t *)cache_obj;
    init_cache(new_cache, sizeof(cache_t), 4096);
    cache_grow(new_cache);

    // add to cache chain
    if (cache_chain == NULL) {
        cache_chain = new_cache;
    } else {
        cache_t *curr = cache_chain;
        while (curr->next != NULL) {
            curr = curr->next;
        }
        curr->next = new_cache;
        new_cache->prev = curr;
    }
}

static void init_cache(cache_t *cache, uint32_t objsize, uint32_t mem) {
    cache->objsize = objsize;
    cache->num = (mem - sizeof(cache_t)) / objsize;
    cache->next = NULL;
    cache->prev = NULL;
    cache->slabs_full = NULL;
    cache->slabs_partial = NULL;
    cache->slabs_empty = NULL;
}

void kmem_init() {
    // create cache of caches, bootstrapped by allocating one page from pmm
    uint32_t cache_addr = vmm_malloc(4096); // allocate two pages 
    cache_cache = (cache_t *)cache_addr;
    init_cache(cache_cache, sizeof(cache_t), 4096);
    // add first slab to cache_cache (slab_cache_grow)
    slab_t *first_cache_slab = (slab_t *)(cache_addr + sizeof(cache_t));
    first_cache_slab->head = (object_t *)(cache_addr + sizeof(cache_t) + sizeof(slab_t));
    first_cache_slab->inuse = 0;
    cache_cache->slabs_empty = first_cache_slab;
    // link objects within slab
    uint32_t curr_cache_addr = cache_addr + sizeof(cache_t) + sizeof(slab_t);
    object_t *curr_cache_obj = (object_t *)first_cache_slab->head;
    while (curr_cache_addr + cache_cache->objsize <= cache_addr + 4096) {
        curr_cache_obj->next = (object_t *)(curr_cache_addr + cache_cache->objsize);
        curr_cache_addr += cache_cache->objsize;
        curr_cache_obj = curr_cache_obj->next;
    }

    // create first cache for slabs
    uint32_t addr = vmm_malloc(4096);
    slab_cache = (cache_t *)addr;
    init_cache(slab_cache, sizeof(slab_t), 4096);
    // create first slab
    slab_t *first_slab = (slab_t *)(addr + sizeof(cache_t));
    first_slab->head = (object_t *)(addr + sizeof(cache_t) + sizeof(slab_t));
    first_slab->inuse = 0;
    slab_cache->slabs_empty = first_slab;
    // link objects within slab
    uint32_t curr_addr = addr + sizeof(cache_t) + sizeof(slab_t);
    object_t *curr_obj = (object_t *)first_slab->head;
    while (curr_addr + slab_cache->objsize <= addr + 4096) {
        curr_obj->next = (object_t *)(curr_addr + slab_cache->objsize);
        curr_addr += slab_cache->objsize;
        curr_obj = curr_obj->next;
    }

    // create general purpose caches
}

uint32_t kmalloc(uint32_t size) {
    // If the requested size is 4 KiB or greater defer to vmm_malloc
    if (size > 2048) {
        return vmm_malloc(size);
    }

    cache_t *curr = cache_chain;
    while (curr != NULL) {
        if (curr->objsize == size) {
            return (uint32_t)object_alloc(curr);
        }
        curr = curr->next;
    }

    return 0;
}

void kfree(uint32_t addr, uint32_t size) {
    if (size > 2048) {
        vmm_free(addr, size);
        return;
    }

    cache_t *curr = cache_chain;
    while (curr != NULL) {
        if (curr->objsize == size) {
            break;
        }
        curr = curr->next;
    }
    if (curr->slabs_full != NULL) {
        // free to full slab
        slab_t *target_slab = curr->slabs_full;
        object_t *obj = (object_t *)addr;
        obj->next = target_slab->head;
        target_slab->head = obj;
        target_slab->inuse--;
        if (target_slab->inuse == curr->num - 1) {
            // move slab to partial list
            curr->slabs_full = NULL;
            curr->slabs_partial = target_slab;
        }
    } else if (curr->slabs_partial != NULL) {
        // free to partial slab
        slab_t *target_slab = curr->slabs_partial;
        object_t *obj = (object_t *)addr;
        obj->next = target_slab->head;
        target_slab->head = obj;
        target_slab->inuse--;
        if (target_slab->inuse == 0) {
            // move slab to empty list
            curr->slabs_partial = NULL;
            curr->slabs_empty = target_slab;
        }
    }
}