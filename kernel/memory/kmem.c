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
    if (cache->slabs_empty == NULL && cache->slabs_partial == NULL) cache_grow(cache);
     
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
}

static void slab_cache_grow(uint32_t base, uint32_t length) {
    slab_t *slab = (slab_t *)(base);
    slab->head = NULL;
    slab->inuse = 0;
    
    uint32_t end = base + length;
    
    base += sizeof(slab_t);

    // Create and link slab objects
    while (base + sizeof(object_t) < end) {
        object_t *obj = (object_t *)base;

        obj->next = slab->head;
        slab->head = obj;

        base += sizeof(object_t);
    }
    
    slab_cache->slabs_empty = slab;
}

static void cache_grow(cache_t *cache) {
   if (slab_cache->slabs_empty == NULL && slab_cache->slabs_partial == NULL) {
        uint32_t *addr = vmm_malloc(4096);

        if (addr == NULL) { /* TODO: handle out of virtual memory */ }

        slab_cache_grow((uint32_t)addr, 4096);
   }

   object_t *slab_obj = object_alloc(slab_cache);

   slab_t *new_slab = (slab_t *)slab_obj;
   new_slab->head = NULL;
   new_slab->inuse = 0;

   uint32_t *addr = vmm_malloc(4096);

   if (addr == NULL) { /* TODO: handle out of virtual memory */ }

   // Create and link 4 KiB of objects for the slab
   uint32_t end = (uint32_t)addr + 4096;

   while ((uint32_t)addr + cache->objsize < end) {
        object_t *obj = (object_t *)addr;

        obj->next = new_slab->head;
        new_slab->head = obj;

        addr = (uint32_t *)((uint32_t)addr + cache->objsize);
   }

   cache->slabs_empty = new_slab;
}

static cache_t *cache_create(uint32_t objsize) {
    if (cache_cache->slabs_empty == NULL && cache_cache->slabs_partial == NULL) cache_grow(cache_cache);

    object_t *cache_obj = object_alloc(cache_cache);

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
    uint32_t addr = (uint32_t)vmm_malloc(4096);

    // Initialize slab struct cache
    slab_cache = (cache_t *)addr;
    cache_init(slab_cache, sizeof(slab_t));
    addr = addr + sizeof(cache_t);
    
    // Initialize the cache struct cache
    cache_cache = (cache_t *)(addr);
    cache_init(cache_cache, sizeof(cache_t));
    addr = addr + sizeof(cache_t);

    // Add first slabs to the slab cache
    slab_cache_grow(addr, 4096 - (2 * sizeof(cache_t)));
    
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

void *kmalloc(uint32_t size) {
    if (size > 2048) return vmm_malloc(size);

    cache_t *curr = cache_chain;

    while (curr != NULL) {
        if (curr->objsize >= size) return object_alloc(curr);

        curr = curr->next;
    }

    return NULL;
}

void kfree(void *obj, uint32_t length) {
    uint32_t addr = (uint32_t)obj;
    
    if (length > 2048) {
        vmm_free(addr, length);
        return;
    }

    cache_t *curr = cache_chain;

    while (curr->next != NULL) {  
        if (curr->objsize >= length) break;

        curr = curr->next;
    }

    // Return the object to its slab in the corresponding cache
    if (curr->slabs_full != NULL) {
        slab_t *target_slab = curr->slabs_full;

        object_t *obj = (object_t *)obj;

        obj->next = target_slab->head;
        target_slab->head = obj;
        target_slab->inuse--;

        curr->slabs_full = NULL;
        curr->slabs_partial = target_slab;
    } else if (curr->slabs_partial != NULL) {
        slab_t *target_slab = curr->slabs_partial;

        object_t *obj = (object_t *)obj;

        obj->next = target_slab->head;
        target_slab->head = obj;
        target_slab->inuse--;

        if (target_slab->inuse == 0) {
            curr->slabs_partial = NULL;
            curr->slabs_empty = target_slab;
        }
    }
}