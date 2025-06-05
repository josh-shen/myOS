#ifndef _PAGING_H
#define _PAGING_H

#include <stdint.h>

struct pte {
    uint32_t present        :1;
    uint32_t rw             :1;
    uint32_t us             :1;
    uint32_t pwt            :1;
    uint32_t pcd            :1;
    uint32_t accessed       :1;
    uint32_t dirty          :1;
    uint32_t pat            :1;
    uint32_t global         :1;
    uint32_t available      :3;
    uint32_t address        :20;
} __attribute__((packed));
typedef struct pte pte_t;

struct pde {
    uint32_t present        :1;
    uint32_t rw             :1;
    uint32_t us             :1;
    uint32_t pwt            :1;
    uint32_t pcd            :1;
    uint32_t accessed       :1;
    uint32_t reserved       :1;
    uint32_t size           :1;
    uint32_t available      :4;
    uint32_t address        :20;
} __attribute__((packed));
typedef struct pde pde_t;

typedef pte_t page_table_t[1024];
typedef pde_t page_directory_t[1024];

#endif