#include <stdint.h>
#include <stdio.h>

#include <memory.h>

static object_t *object_alloc(cache_t *);
static void slab_cache_grow(uint32_t, uint32_t);
static void cache_grow(cache_t *);
static cache_t *cache_create(uint32_t);
static void cache_init(cache_t *, uint32_t);

static cache_t *cache_chain = NULL;
static cache_t *cache_cache = NULL;
static cache_t *slab_cache = NULL;

static object_t *object_alloc(cache_t *cache) {
    if (cache->slabs_empty == NULL && cache->slabs_partial == NULL) {
        cache_grow(cache);
    }
     
    if (cache->slabs_partial != NULL) {
        slab_t *target_slab = cache->slabs_partial;

        object_t *obj = target_slab->head;

        target_slab->head = obj->next;
        target_slab->inuse++;

        if (target_slab->inuse == cache->num) {
            cache->slabs_partial = NULL;
            cache->slabs_full = target_slab;
        }

        return obj;
    } else if (cache->slabs_empty != NULL) {
        slab_t *target_slab = cache->slabs_empty;

        object_t *obj = target_slab->head;

        target_slab->head = obj->next;
        target_slab->inuse++;

        if (target_slab->inuse == cache->num) {
            cache->slabs_empty = NULL;
            cache->slabs_full = target_slab;
        } else {
            cache->slabs_empty = NULL;
            cache->slabs_partial = target_slab;
        }

        return obj;
    }

    return NULL; // TODO: error handling for no available slabs
}

static void slab_cache_grow(uint32_t base, uint32_t length) {
    slab_t *slab = (slab_t *)(base);
    slab->head = NULL;
    slab->inuse = 0;

    base += sizeof(slab_t);

    // Create and link slab objects
    uint32_t end = base + length;

    while (base <= end) {
        object_t *obj = (object_t *)base;

        obj->next = slab->head;
        slab->head = obj;

        base += sizeof(object_t);
    }
    
    slab_cache->slabs_empty = slab;
}

static void cache_grow(cache_t *cache) {
   if (slab_cache->slabs_empty == NULL && slab_cache->slabs_partial == NULL) {
        uint32_t addr = vmm_malloc(4096);

        slab_cache_grow(addr, 4096);
   }

   object_t *slab_obj;

   // Get a new slab from the slab cache
   if (slab_cache->slabs_partial != NULL) {
        slab_t *slab = slab_cache->slabs_partial;

        slab_obj = slab->head;

        slab->head = slab->head->next;
        slab->inuse++;

        if (slab->inuse == slab_cache->num) {
            slab_cache->slabs_partial = NULL;
            slab_cache->slabs_full = slab;
        }
   } else if (slab_cache->slabs_empty != NULL) {
        slab_t *slab = slab_cache->slabs_empty;

        slab_obj = slab->head;

        slab->head = slab->head->next;
        slab->inuse++;

        slab_cache->slabs_empty = NULL;
        if (slab->inuse == slab_cache->num) {
            slab_cache->slabs_full = slab;
        } else {
            slab_cache->slabs_partial = slab;
        }
   }

   slab_t *new_slab = (slab_t *)slab_obj;
   new_slab->head = NULL;
   new_slab->inuse = 0;

   uint32_t addr = vmm_malloc(4096);

   // Create and link 4 KiB of objects for the slab
   uint32_t end = addr + 4096;

   while (addr <= end) {
        object_t *obj = (object_t *)addr;

        obj->next = new_slab->head;
        new_slab->head = obj;

        addr += sizeof(object_t);
   }

   cache->slabs_empty = new_slab;
}

static cache_t *cache_create(uint32_t objsize) {
   if (cache_cache->slabs_empty == NULL && cache_cache->slabs_partial == NULL) {
       cache_grow(cache_cache);
   }

   object_t *cache_obj;

   if (cache_cache->slabs_partial != NULL) {
       slab_t *cache_slab = cache_cache->slabs_partial;

       cache_obj = cache_slab->head;

       cache_slab->head = cache_obj->next;
       cache_slab->inuse++;
       if (cache_slab->inuse == cache_cache->num) {
           cache_cache->slabs_partial = NULL;
           cache_cache->slabs_full = cache_slab;
       }
   } else if (cache_cache->slabs_empty != NULL) {
       slab_t *cache_slab = cache_cache->slabs_empty;

       cache_obj = cache_slab->head;

       cache_slab->head = cache_obj->next;
       cache_slab->inuse++;

       cache_cache->slabs_empty = NULL;
       if (cache_slab->inuse == cache_cache->num) {
           cache_cache->slabs_full = cache_slab;
       } else {
           cache_cache->slabs_partial = cache_slab;
       }
   }

    cache_t *new_cache = (cache_t *)cache_obj;
    cache_init(new_cache, objsize);
    cache_grow(new_cache);

    return new_cache;
}

static void cache_init(cache_t *cache, uint32_t objsize) {
    cache->objsize = objsize;
    cache->num = 4096 / objsize;
    cache->next = NULL;
    cache->slabs_full = NULL;
    cache->slabs_partial = NULL;
    cache->slabs_empty = NULL;
}

void kmem_init() {
    // Allocate one page for slab struct cache and and the cache's slab objects
    uint32_t addr = vmm_malloc(4096);
    
    // Initialize slab struct cache
    slab_cache = (cache_t *)addr;
    cache_init(slab_cache, sizeof(slab_t));
    addr += sizeof(cache_t);
    
    // Initialize the cache struct cache
    cache_cache = (cache_t *)(addr + sizeof(cache_t));
    cache_init(cache_cache, sizeof(cache_t));
    addr += sizeof(cache_t);
    
    // Add first slabs to the slab cache
    slab_cache_grow(addr, 4096 - (2 * sizeof(cache_t))); // TODO: page fault here
    
    // Use slab cache to grow cache cache
    cache_grow(cache_cache);

    // Create general purpose caches for sizes 2^5 to 2^11
    for (int i = 11; i >= 5; i--) {
        cache_t *cache = cache_create(1 << i);

        if (cache_chain == NULL) cache_chain = cache;

        cache->next = cache_chain;
        cache_chain = cache;
    }
}

uint32_t kmalloc(uint32_t size) {
    if (size > 2048) {
        return vmm_malloc(size);
    }

    cache_t *curr = cache_chain;

    while (curr != NULL) {
        if (curr->objsize >= size) {
            return (uint32_t)object_alloc(curr);
        }
        curr = curr->next;
    }

    return 0;
}

void kfree(void *obj) {
    uint32_t addr = (uint32_t)obj;
    uint32_t size = sizeof(obj);
    
    if (size > 2048) {
        vmm_free(addr, size);
        return;
    }

    cache_t *curr = cache_chain;

    while (curr != NULL) {
        if (curr->objsize >= size) break;

        curr = curr->next;
    }

    // Return the object to its slab in the corresponding cache
    if (curr->slabs_full != NULL) {
        slab_t *target_slab = curr->slabs_full;

        object_t *obj = (object_t *)addr;

        obj->next = target_slab->head;
        target_slab->head = obj;
        target_slab->inuse--;

        curr->slabs_full = NULL;
        curr->slabs_partial = target_slab;
    } else if (curr->slabs_partial != NULL) {
        slab_t *target_slab = curr->slabs_partial;

        object_t *obj = (object_t *)addr;

        obj->next = target_slab->head;
        target_slab->head = obj;
        target_slab->inuse--;

        if (target_slab->inuse == 0) {
            curr->slabs_partial = NULL;
            curr->slabs_empty = target_slab;
        }
    }
}